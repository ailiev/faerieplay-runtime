#include <string>
#include <algorithm>		// for swap(), max and min

#include <math.h>

#include <boost/shared_ptr.hpp>
#include <boost/optional/optional.hpp> 
#include <boost/none.hpp>


#include <pir/common/sym_crypto.h>
#include <pir/card/lib.h>
#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/io_filter_encrypt.h>

#include <stream/processor.h>
#include <stream/helpers.h>

#include "utils.h"
#include "array.h"
#include "batcher-permute.h"




using std::string;
using std::auto_ptr;
using std::pair;

using boost::shared_ptr;
using boost::optional;

using std::min;
using std::max;



using boost::optional;
using boost::none;


// options to use a watered down algorithm, if the full and proper one is
// failing.
// #define NO_REFETCHES
// #define NO_ENCRYPT

// how to examine the working area indices files on the host:
// for f in *; do echo $f; od -t d4 -A n $f; done
// and the working area objects:
// for f in *; do echo $f; od -s -A n $f; done
// of course NO_ENCRYPT should be enabled for this to be useful.

namespace {
    Log::logger_t logger = Log::makeLogger ("array",
					    boost::none, boost::none);    
}




namespace {
    // instantiate the static array map
    Array::map_t Array::_arrays;
    Array::des_t Array::_next_array_num = 1;

    const string ARRAY_NAME = "-array",
	IDXS_NAME = "-working-idxs",
	OBJS_NAME = "-working-objs";
}


// the static logger
Log::logger_t Array::_logger;

INSTANTIATE_STATIC_INIT(Array);


// Containers used:
// 
// array: the encrypted but unpermuted array values. may or may not exists when
// the Array object is instantiated:
// - exists if previusly created eg. as part of input processing.
// - does not exist if eg. doing an gate_t::InitDynArray gate now
//
// touched: the elements accessed this session, encrypted and kept sorted (by
// us)
// does not exist when the Array object is created.


/// @param size_params ->first is the array length, ->second is the element
/// size. boost::none if the array should already exist on the host.
Array::Array (const string& name,
	      const boost::optional <pair<size_t, size_t> >& size_params,
              CryptoProviderFactory * prov_fact)
    : _name		(name),
      
      N			(size_params ? size_params->first  : 0),
      _elem_size	(size_params ? size_params->second : 0),
      
      _prov_fact	(prov_fact),
      _rand_prov	(_prov_fact->getRandProvider()),

      _Ts		(1),
      _As		(1),
      
      _num_branches	(1),
      _num_retrievals	(0)

{

    _As[0] = new ArrayA (name, size_params, &N, &_elem_size);

    // if the array exists, get its sizes, and fill in the size-dependent
    // params.
    if (!size_params) {
	N 	   = _As[0]._io.getLen();
	_elem_size = _As[0]._io.getElemSize();
    }

    _max_retrievals = lrint (sqrt(float(N))) *  lgN_floor(N);
    // this happens for very small N
    if (_max_retrievals >= N) {
	_max_retrievals = N-1;
    }

    // init to identity permutation, and then do repermute()
    _p 		    = auto_ptr<TwoWayPermutation> (new IdPerm (N));

    _Ts[0] = new ArrayT (_name, _max_retrievals, _elem_size, &_num_retrievals);

    // FIXME: may not want to repermute, if user is filling out the array with
    // clear_write(), and wants to re-permute it thereafter.
    repermute();
}



void Array::write_clear (index_t idx, const ByteBuffer& val)
    throw (host_exception, comm_exception)
{
    _array_io.write (idx, val);
}




ByteBuffer Array::read (index_t idx, index_t branch)
    throw (better_exception)
{
    // need to:
    // 
    // - add either idx or (rand_idx <- a random untouched index) to the touched
    //   list
    // - read all elements in the touched list, while keeping A[idx]
    // - return A[idx] to caller


    index_t p_idx = _p->p(idx);

#ifdef NO_REFETCHES
    ByteBuffer obj;
    _array_io.read (p_idx, obj);
    _num_retrievals++;
    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
    return obj;
#endif
    
    append_new_working_item (p_idx, branch);

    ByteBuffer buf = _Ts[branch]->do_dummy_accesses (p_idx,
						     boost::none);
    
    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }

    return buf;
}


void Array::write (index_t idx, size_t off,
		   const ByteBuffer& val)
    throw (better_exception)
{
    
    index_t p_idx = _p->p(idx);

#ifdef NO_REFETCHES
    _array_io.write (p_idx, val);
    _num_retrievals++;
    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
    return;
#endif
    
    append_new_working_item (p_idx);

    
    do_dummy_fetches (p_idx,
		      std::make_pair (off, val));

    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
}


class Array::dummy_fetches_stream_prog
{

    // named indexers into the above arrays
    enum {
	IDX=0,			// the item index
	ITEM=1			// the actual item
    };
	    
public:

    dummy_fetches_stream_prog (index_t target_idx,
			       const optional <pair<size_t, ByteBuffer> > & new_val,
			       elem_size)
	: _target_idx (target_idx),
	  _new_val (new_val),
	  _elem_size (elem_size)
	{}
    
    void operator() (const boost::array<index_t,2>& idxs,
		     const boost::array<ByteBuffer,2>& objs,
		     boost::array<ByteBuffer,2>& o_objs)
	{
	    index_t 		T_i  = bb2basic<index_t> (objs[IDX]);
	    const ByteBuffer & 	item =			  objs[ITEM];

	    // do this in case the output goes to a different container
	    o_objs[IDX] = objs[IDX];
	    
	    if (T_i == _target_idx) {
		assert (((void)"dummy_fetches_stream_prog should not "
			 "see its target index more than once",
			 _the_item.len() == 0));

		_the_item = ByteBuffer (item, ByteBuffer::deepcopy());

		if (_new_val) {
		    // all sizes need to be the same! caller should ensure this
		    assert (_new_val->second.len() == _elem_size);
		
		    ByteBuffer towrite = _new_val->second;
		    // FIXME: ignoring the offset (new_val->first) for now
		    o_objs[ITEM] = towrite;
		}
		else {
		    // pass through
		    o_objs[ITEM] = objs[ITEM];
		}
	    }

	    else {
		// pass through, but will be written out re-encrypted with a new IV
		o_objs[ITEM] = objs[ITEM];
	    }
	    
	}

    ByteBuffer getTheItem ()
	{
	    assert (((void)"dummy_fetches_stream_prog should have found the target item",
		     _the_item.len() > 0));

	    return _the_item;
	}
    
private:

    index_t _target_idx;
    const optional <pair<size_t, ByteBuffer> > & _new_val;

    size_t _elem_size;

    ByteBuffer _the_item;
};




/// retrieve or update an object, while doing all the necesary re-fetches.
/// @param idx the physical index to read/write
/// @param new_val pointer to an offset and data value, to be written to
///       A[idx] if it is non-null
/// @return the object at #idx
ByteBuffer Array::do_dummy_fetches (index_t idx,
				    optional < pair<size_t, ByteBuffer> > new_val)
    throw (better_exception)
{
    boost::array<FlatIO*,2> in_ios =  { &_idxs, &_items };
    boost::array<FlatIO*,2> out_ios = { &_idxs, new_val ? &_items : NULL };
    
    dummy_fetches_stream_prog prog (idx, new_val, this);

    stream_process (prog,
		    zero_to_n<2> (_num_retrievals),
		    in_ios,
		    out_ios,
		    boost::mpl::size_t<1> (), // number of items/idxs in an
					      // invocation of prog
		    boost::mpl::size_t<2> ()); // the number of I/O streams

    return prog.getTheItem ();
}



void Array::repermute ()
{

#ifndef NO_REFETCHES
    // copy values out of the working area and into the main array
    for (unsigned b=0; b < _As.size(); b++)
    {
	if (_As[b])
	{
	    merge (_Ts[b], _As[b]);
	}
    }
#endif // NO_REFETCHES
	
    // if we only have one ArrayA and multiple branches, need to duplicate the
    // ArrayA
    if (_num_branches > 1 && _As.size() == 1)
    {
	_As.resize (_Ts.size());
	
	// HACK: get the valid branch numbers by iterating through _Ts and
	// seeing if a T is at each i
	for (unsigned i=1; i < _Ts.size(); i++)
	{
	    if (_Ts[i])
	    {
		_As[i] = _As[0].duplicate (i);
	    }
	}
    }

    // the new permutation
    auto_ptr<TwoWayPermutation> p2 (new LRPermutation (
					lgN_ceil(N), 7, _prov_fact));
    p2->randomize();

    repermute_As (_p, p2);
    
    _p = p2;

    _num_retrievals = 0;
}



struct rand_idx_and_have_idx
{
    rand_idx_and_have_idx (index_t target_index,
			   index_t rand_idx_initval)
	: target_index	(target_index),
	  rand_idx	(rand_idx_initval),
	  have_target	(false),
	  have_rand	(false)
	{}
    
    void operator() (index_t, const ByteBuffer& in, ByteBuffer&)
	{
	    index_t T_i = bb2basic<index_t> (in);

	    if (T_i == target_index)
	    {
		have_target = true;
	    }
	    if (T_i == rand_idx)
	    {
		have_rand = true;
	    }
	}

    const index_t target_index;
    const index_t rand_idx;
    bool have_target;
    bool have_rand;		// is the proposed rand_idx in the working set
				// already?
};



/// add a new distinct element to T:  either idx or a random
/// index (not already in T).
///
/// working with physical indices here
void Array::append_new_working_item (index_t idx, index_t branch)
    throw (better_exception)
{
    index_t rand_idx;
    optional<index_t> to_append;
    while (!to_append)
    {
	rand_idx = _rand_prov->randint (N);
	LOG (Log::DEBUG, logger,
	     "While accessing physical idx " << idx << ", trying rand_idx " << rand_idx);
	to_append = _Ts[branch]->find_fetch_idx (rand_idx, idx);
    }

    // grab the object at to_append
    ByteBuffer obj;
    getA(branch)->_io.read (*to_append, obj);

    // append it to all our Ts
    for (unsigned i=0; i < _Ts.size(); i++)
    {
	if (_Ts[i]) _Ts[i]->appendItem (*to_append, obj);
    }
	
    _num_retrievals++;
}




/*
  Scopes and Select gates:

  Upon hitting an array op at a depth larger than the array's source gate, fork
  off a copy to work on.

  Then, when a Select for the array comes, the two versions will be distinct.
  The selection will either iterate through the working areas of both arrays, or
  the whole storage, if a re-permutation has taken place in the mean time.

  That's it!

  But now: how do I keep the working area separate, so it can develop
  independently in 2 copies of the array, but be merge-able linearly in the
  touched set size, not the whole array?
  - if T is the same size in both branches, no problem to iterate along both
  _touched_io containers and select the chosen array's accesses.
  - but if T is different size?
     - we need to hide which size T is selected, so the result array's T must be
       the bigger of the two inputs. what if array with the smaller T is
       selected? seems problematic - subsequent re-fetches could reveal which A
       was selected. in fact, we need to construct a T which is the
       *concatenation* of the two input A's Ts. in what order? no matter - the
       order given in the gate should do.
     - what if the sum of T's is bigger than _max_retrievals? do a select out of
       the entire A, and then repermute?
  - which permutation do we use? the selected array's?
*/




void Array::repermute_As(const shared_ptr<TwoWayPermutation>& old_p,
			 const shared_ptr<TwoWayPermutation>& new_p)
{
    FOREACH (A, _As) {
	if (*A) (*A)->repermute (old_p, new_p);
    }
}


//
//
// class ArrayT
//
//

Array::ArrayT::ArrayT (const std::string& name, /// the array name
		       size_t max_retrievals,
		       size_t elem_size,
		       const size_t * p_num_retrievals,
		       CryptoProviderFactory * prov_fact)
    : _array_name (name),
      _branch (0),
      _idxs (_array_name + DIRSEP + "touched" + DIRSEP + "branch-0-idxs",
	     Just (std::make_pair (max_retrievals, sizeof(index_t)))),
      _items (_array_name + DIRSEP + "touched" + DIRSEP + "branch-0-items",
	      Just (std::make_pair (max_retrievals, elem_size))),
      _num_retrievals (p_num_retrievals)
{
    // add encrypt/decrypt filters to all the IO objects.
    FlatIO * ios [] = { &_idxs, &_items };
    for (unsigned i=0; i < ARRLEN(ios); i++) {
#ifndef NO_ENCRYPT
	ios[i]->appendFilter (
	    auto_ptr<HostIOFilter>
	    (new IOFilterEncrypt (ios[i],
				  shared_ptr<SymWrapper>
				  (new SymWrapper (prov_fact)))));
#endif
    }
    
}


boost::optional<index_t>
Array::ArrayT::
find_fetch_idx (index_t rand_idx, index_t target_idx)
{
    rand_idx_and_have_idx prog (target_idx, rand_idx);
    stream_process (prog,
		    zero_to_n (*_num_retrievals),
		    &_idxs,
		    NULL);
    
    return prog.have_rand ?
	(prog.have_target ? rand_idx : target_idx) :
	boost::none;
}

ByteBuffer
Array::ArrayT::
do_dummy_accesses (index_t target_index,
		   const boost::optional < std::pair<size_t, ByteBuffer> >& new_val)
{
    boost::array<FlatIO*,2> in_ios =  { &_idxs, &_items };
    boost::array<FlatIO*,2> out_ios = { &_idxs, new_val ? &_items : NULL };
    
    dummy_fetches_stream_prog prog (target_index, new_val,
				    _items.getElemSize());

    stream_process (prog,
		    zero_to_n<2> (*_num_retrievals),
		    in_ios,
		    out_ios,
		    boost::mpl::size_t<1> (), // number of items/idxs in an
					      // invocation of prog
		    boost::mpl::size_t<2> ()); // the number of I/O streams

    return prog.getTheItem ();
}

void
Array::ArrayT::
appendItem (index_t idx, const ByteBuffer& item)
{
    _idxs.write (*_num_retrievals, basic2bb (idx));
    _items.write (*_num_retrievals, item);
}

ArrayT
Array::ArrayT::
duplicate (index_t new_branch)
{
    return ArrayT (*this, new_branch);
}

Array::ArrayT::ArrayT (const ArrayT& b,
		       unsigned branch_num) // this will be the b-th branch

    : _array_name (b._array_name),
      _branch (branch_num),
      _idxs (_array_name + DIRSEP + "touched" + DIRSEP + "branch-" +
	     itoa(_branch) + "-idxs",
	     Just (std::make_pair (b._idxs.getLen(), b._idxs.getElemSize()))),
      _items (_array_name + DIRSEP + "touched" + DIRSEP + "branch-" +
	      itoa(_branch) + "-items",
	      Just (std::make_pair (b._items.getLen(), b._items.getElemSize()))),
      _num_retrievals (b._num_retrievals)
{
    // need to copy the elements across
    array<FlatIO*,2> in_ios = { &b._idxs, &b._items };
    array<FlatIO*,2> out_ios = { &_idxs, &_items };

    stream_process (identity_itemproc<2>,
		    zero_to_n<2> (_num_retrievals),
		    in_ios,
		    out_ios,
		    boost::mpl::size_t<1> (),
		    boost::mpl::size_t<2> ());
}



//
// class ArrayA
//

Array::ArrayA::ArrayA (const std::string& name, /// the array name
		       const optional<pair<size_t, size_t> >& size_params,
		       CryptoProviderFactory *  prov_fact)
    : _io (name + DIRSEP + "array-0",
	   size_params)
	   
{
#ifndef NO_ENCRYPT
    // add encrypt/decrypt filter.
    _io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (&_io,
			      shared_ptr<SymWrapper>
			      (new SymWrapper (prov_fact)))));
#endif

    // if array is new, fill out with nulls.
    if (size_params)
    {
	ByteBuffer zero (size_params->second); // the element size
	zero.set (0);
	stream_process (make_generate_proc_adapter (make_fconst(zero)),
			zero_to_n (size_params->first),	// array length
			NULL,
			&_io);
    }
}


void
Array::ArrayA::repermute (const shared_ptr<TwoWayPermutation>& old_p,
			  const shared_ptr<TwoWayPermutation>& new_p,
			  CryptoProviderFactory * prov_fact)
{
    // a container for the new permuted array.
    // its IOFilterEncrypt will setup the new keys
    shared_ptr<FlatIO> p2_cont_io (new FlatIO (_io.getName() + "-p2",
					       std::make_pair (*N, *_elem_size)));
#ifndef NO_ENCRYPT
    p2_cont_io->appendFilter (
	auto_ptr<HostIOFilter> (
	    new IOFilterEncrypt (
		p2_cont_io.get(),
		new SymWrapper (prov_fact))));
#endif

    // copy values across
    stream_process ( identity_itemproc,
		     zero_to_n (*N),
		     &_io,
		     &(*p2_cont_io) );

    // run the re-permutation on the new container
    shared_ptr<ForwardPermutation> reperm (new RePermutation (old_p, new_p));

    Shuffler shuffler  (p2_cont_io, reperm, *N);
    shuffler.shuffle ();

    // install the new parameters
    _io = *p2_cont_io;
}    




//
// class ArrayHandle
//

ArrayHandle::des_t ArrayHandle::newArray (const std::string& name,
					  size_t len, size_t elem_size,
					  CryptoProviderFactory * crypt_fact)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name,
					 Just (std::make_pair (len, elem_size)),
					 crypt_fact));

    insert_arr (arrptr);
}

ArrayHandle::des_t ArrayHandle::newArray (const std::string& name,
					  CryptoProviderFactory * crypt_fact)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name,
					 none,
					 crypt_fact));

    insert_arr (arrptr);
}


ArrayHandle::des_t
ArrayHandle::insert_arr (const boost::shared_ptr<Array>& arr)
{
    des_t idx = _next_array_num++;

    // there should be no entry with idx
    assert (_arrays.find(idx) == _arrays.end() );
    
    _arrays[idx] = ArrayHandle (arrptr, 0); // branch 0

    return idx;
}    


ArrayHandle & ArrayHandle::getArray (ArrayHandle::des_t desc)
{
    map_t::iterator arr_i = _arrays.find (arr);
    if (arr_i == _arrays.end()) {
	throw bad_arg_exception ("Array::getArray: non-existent array requested");
    }

    return arr_i->first;
}



ArrayHandle &
ArrayHandle::write (unsigned depth,
		    index_t idx, size_t off,
		    const ByteBuffer& val);
throw (better_exception)
{
    if (depth > _last_depth)
    {
	// new branch!
	index_t new_branch = _arr->newBranch (_branch);

	// add a new Handle with this branch number to the map
	des_t idx = _next_array_num++;
	ArrayHandle newh (_arr, new_branch, depth, idx);
	_arrays[idx] = newh;

	return newh.write (depth, idx, off, val);
    }

    _arr->write (idx, off, _branch, val);
    return *this;
}


/// Read the value from an array index.
/// Encapsulates all the re-fetching and permuting, etc.
/// @param idx the index
/// @return new ByteBuffer with the value
ArrayHandle&
ArrayHandle::read (unsigned depth, index_t i,
		   ByteBuffer & out);
throw (better_exception)
{
    if (depth > _last_depth)
    {
	// new branch!
	index_t new_branch = _arr->newBranch (_branch);

	// add a new Handle with this branch number to the map
	des_t idx = _next_array_num++;
	ArrayHandle newh (_arr, new_branch, depth, idx);
	_arrays[idx] = newh;
	// pass on the read to the new guy
	return newh.read (depth, i, out);
    }

    out = _arr->read (i, _branch);
    return *this;
}
