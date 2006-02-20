#include <string>
#include <algorithm>		// for swap(), max and min

#include <math.h>

#include <boost/shared_ptr.hpp>
#include <boost/optional/optional.hpp> 
#include <boost/none.hpp>
#include <boost/array.hpp>

#include <pir/common/sym_crypto.h>
#include <pir/card/lib.h>
#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/io_filter_encrypt.h>

#include "stream/processor.h"
#include "stream/helpers.h"

#include "utils.h"
#include "array.h"
#include "batcher-permute.h"
#include "runtime-exceptions.h"


OPEN_NS

using std::string;
using std::auto_ptr;
using std::pair;
using std::make_pair;

using boost::shared_ptr;
using boost::optional;
using boost::array;

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


// namespace {
//     Log::logger_t logger = Log::makeLogger ("array",
// 					    boost::none, boost::none);    
// }




namespace {
    // instantiate the static array map
    ArrayHandle::map_t ArrayHandle::_arrays;
    ArrayHandle::des_t ArrayHandle::_next_array_num = 1;
}


// the static logger
Log::logger_t Array::_logger;

INSTANTIATE_STATIC_INIT(Array);




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

    _As[0] = shared_ptr<ArrayA> (new ArrayA (name,
					     size_params, &N, &_elem_size,
					     _prov_fact));

    // if the array exists, get its sizes, and fill in the size-dependent
    // params.
    if (!size_params) {
	N 	   = _As[0]->_io.getLen();
	_elem_size = _As[0]->_io.getElemSize();
    }

    _max_retrievals = lrint (sqrt(float(N))) *  lgN_floor(N);
    // this happens for very small N
    if (_max_retrievals >= N) {
	_max_retrievals = N-1;
    }

    // init to identity permutation, and then do repermute()
    _p 		    = shared_ptr<TwoWayPermutation> (new IdPerm (N));

    _Ts[0] = shared_ptr<ArrayT> (
	new ArrayT (_name,
		    _max_retrievals, _elem_size,
		    &_num_retrievals,
		    _prov_fact));
}



/// write a value directly to branch 0, without any permutation etc.
void Array::write_clear (index_t idx, const ByteBuffer& val)
    throw (host_exception, comm_exception)
{
    _As[0]->_io.write (idx, val);
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


    LOG (Log::DEBUG, _logger,
	 "Array::read idx=" << idx << "; branch=" << branch);

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
		   index_t branch,
		   const ByteBuffer& val)
    throw (better_exception)
{
    
    LOG (Log::DEBUG, _logger,
	 "Array::write idx=" << idx << "; branch=" << branch);
    
    index_t p_idx = _p->p(idx);

#ifdef NO_REFETCHES
    _array_io.write (p_idx, val);
    _num_retrievals++;
    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
    return;
#endif
    
    append_new_working_item (p_idx, branch);

    
    (void) _Ts[branch]->do_dummy_accesses (p_idx,
					   std::make_pair (off, val));

    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
}



/// do a select operation, and return the branch index of the result.
/// @param sel_first if true select the first branch, if false the second one.
index_t
Array::select (index_t a, index_t b, bool sel_first)
{
    LOG (Log::DEBUG, _logger,
	 "Array::select on branches " << a << " and " << b
	 << " with sel_first = " << sel_first);
    
    if ( !_Ts[a] || !_Ts[b] ||
	 (have_mult_As() && (!_As[a] || !_As[b])) )
    {
	throw illegal_operation_argument ("select on non-existant arrays " +
					  itoa(a) + ", " + itoa(b));
    }
    
//    index_t result_idx = min (a, b);

    index_t
	sel   = sel_first ? a : b,
	unsel = sel_first ? b : a;
    
    select (*_Ts[a], *_Ts[b], sel_first);
//    _Ts[result_idx] = _Ts[sel];
    _Ts[unsel]      = shared_ptr<ArrayT> (); // set to an empty shared_ptr
    
    if (have_mult_As()) {
	// The A's have been cloned, so we need to merge them too
	select (*_As[a], *_As[b], sel_first);
//	_As[result_idx] = _As[sel];
	_As[unsel] = shared_ptr<ArrayA> ();
    }

    return sel;
}




void Array::repermute ()
{
    LOG (Log::PROGRESS, _logger,
	 "Array::repermute() on array " << _name);
    
    // duplicate all the A's which have corresponding T's
    _As.resize (_Ts.size());
	
    FOREACH (newb, _As_to_duplicate) {
	index_t
	    src_b  = newb->first,
	    dest_b = newb->second;
	_As[dest_b] = _As[src_b]->duplicate(dest_b);
    }
    _As_to_duplicate.clear();

#ifndef NO_REFETCHES
    // copy values out of the working area and into the main array
    for (unsigned b=0; b < _As.size(); b++)
    {
	if (_As[b])
	{
	    merge (*_Ts[b], *_As[b]);
	}
    }
#endif // NO_REFETCHES
	
    // the new permutation
    shared_ptr<TwoWayPermutation> p2 (new LRPermutation (
					  lgN_ceil(N), 7, _prov_fact));
    p2->randomize();

    repermute_As (_p, p2);
    
    _p = p2;

    _num_retrievals = 0;
}


// make a new branch and return its index
index_t
Array::newBranch (index_t source_branch)
{
    // find the first free branch, or push a new one one if no free one
    unsigned b;
    for (b=0; b < _Ts.size() && _Ts[b]; b++)
	;
    if (b == _Ts.size())
    {
	_Ts.resize (_Ts.size()+1);
    }
	
    // duplicate the T
    _Ts[b] = _Ts[source_branch]->duplicate (b);

    // NOTE: if we're in mulitple A's mode, the A will have to be duplicated at
    // next re-permute.
    // FIXME: except if the T's are merged before the repermute! then have to
    // cancel this A's duplication.
    _As_to_duplicate.push_back (make_pair (source_branch, b));

    return b;
}
    



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
	LOG (Log::DEBUG, _logger,
	     "While accessing physical idx " << idx << ", trying rand_idx " << rand_idx);
	to_append = _Ts[branch]->find_fetch_idx (rand_idx, idx);
    }

    // grab the object at to_append
    ByteBuffer obj;
    {
	// FIXME: if the T at branch was branched from another b_2, but the A at
	// b_2 has not yet been duplicated, this won't fly - we'll need to
	// read the obj from A at b_2, or even further up a stack of T branchings.
	//
	// solution: store a T's associated A.
	ArrayA &  arr = have_mult_As() ? *_As[branch] : *_As[0];
	arr._io.read (*to_append, obj);
    }

    // append it to all our Ts
    for (unsigned i=0; i < _Ts.size(); i++)
    {
	if (_Ts[i]) _Ts[i]->appendItem (*to_append, obj);
    }
	
    _num_retrievals++;
}


/// merge a T into an A
void
Array::merge (const ArrayT& T, ArrayA& A)
{
    for (unsigned i=0; i < *T._num_retrievals; i++)
    {
	ByteBuffer item, idxbuf;
	T._idxs .read (i, idxbuf);
	T._items.read (i, item);

	A._io.write (bb2basic<index_t>(idxbuf), item);
    }
}




void Array::repermute_As(const shared_ptr<TwoWayPermutation>& old_p,
			 const shared_ptr<TwoWayPermutation>& new_p)
{
    LOG (Log::DEBUG, _logger,
	 "Array::repermute_As() called");
    
    FOREACH (A, _As) {
	if (*A) (*A)->repermute (old_p, new_p);
    }
}


// stream program to select one of two I/O streams. The output only goes into
// the first stream, the second is not used and can be set to NULL.
struct selector_prog
{
    selector_prog (bool sel_first)
	: sel_first (sel_first)
	{}
    
    void operator() (const array<index_t,2>&,
		     const array<ByteBuffer,2> & in,
		     array<ByteBuffer,2> & out) const
	{
	    out[0] = sel_first ? in[0] : in[1];
	    // out[1] is not used
	}

    bool sel_first;
};
		

// select the specified T, updating it in place
void
Array::select (ArrayT& a, ArrayT& b, bool sel_first)
{
    LOG (Log::DEBUG, _logger,
	 "Array::select(T) called");
    
    FlatIO * selpairs[2][2] = { { &a._idxs, &b._idxs },
				{ &a._items, &b._items } };

    for (unsigned i=0; i < 2; i++)
    {
	FlatIO ** selpair = selpairs[i];
	
	array<FlatIO*,2> in_ios =  { selpair[0], selpair[1] };
	array<FlatIO*,2> out_ios = { sel_first ? selpair[0] : selpair[1], NULL };
	
	stream_process (selector_prog (sel_first),
			zero_to_n<2> (*a._num_retrievals),
			in_ios,
			out_ios,
			boost::mpl::size_t<1>(),
			boost::mpl::size_t<2>());
    }
}

// select the specified A, updating it in place
void
Array::select (ArrayA& a, ArrayA& b, bool sel_first)
{
    array<FlatIO*,2> in_ios =  { &a._io, &b._io };
    array<FlatIO*,2> out_ios = { sel_first ? &a._io : &b._io, NULL };

    stream_process (selector_prog (sel_first),
		    zero_to_n<2> (*a.N),
		    in_ios,
		    out_ios,
		    boost::mpl::size_t<1>(),
		    boost::mpl::size_t<2>());
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
    : _name		(name),
      _idxs		(_name + DIRSEP + "touched" + DIRSEP + "branch-0-idxs",
			 Just (std::make_pair (max_retrievals, sizeof(index_t)))),
      _items		(_name + DIRSEP + "touched" + DIRSEP + "branch-0-items",
			 Just (make_pair (max_retrievals, elem_size))),
      _num_retrievals	(p_num_retrievals),
      _prov_fact	(prov_fact)
{
#ifndef NO_ENCRYPT
    // add encrypt/decrypt filters to all the IO objects.
    FlatIO * ios [] = { &_idxs, &_items };
    for (unsigned i=0; i < ARRLEN(ios); i++) {
	ios[i]->appendFilter (
	    auto_ptr<HostIOFilter>
	    (new IOFilterEncrypt (ios[i],
				  shared_ptr<SymWrapper>
				  (new SymWrapper (_prov_fact)))));
    }
#endif
}



Array::ArrayT::ArrayT (const ArrayT& b,
		       unsigned branch) // this will be the b-th branch

    : _name		(b._name),
      _idxs		(_name + DIRSEP + "touched" + DIRSEP + "branch-" +
			 itoa(branch) + "-idxs",
			 Just (make_pair (b._idxs.getLen(),
					  b._idxs.getElemSize()))),
      _items		(_name + DIRSEP + "touched" + DIRSEP + "branch-" +
			 itoa(branch) + "-items",
			 Just (make_pair (b._items.getLen(),
					  b._items.getElemSize()))),
      _num_retrievals	(b._num_retrievals),
      _prov_fact	(b._prov_fact)
{
#ifndef NO_ENCRYPT
    // add encrypt/decrypt filters to all the IO objects.
    FlatIO * ios [] = { &_idxs, &_items };
    for (unsigned i=0; i < ARRLEN(ios); i++)
    {
	ios[i]->appendFilter (
	    auto_ptr<HostIOFilter>
	    (new IOFilterEncrypt (ios[i],
				  shared_ptr<SymWrapper>
				  (new SymWrapper (_prov_fact)))));
    }
#endif

    // need to copy the elements across
    array<FlatIO*,2> in_ios =  { &b._idxs, &b._items };
    array<FlatIO*,2> out_ios = { &_idxs,   &_items };

    stream_process (identity_itemproc<2>(),
		    zero_to_n<2> (*_num_retrievals),
		    in_ios,
		    out_ios,
		    boost::mpl::size_t<1> (),
		    boost::mpl::size_t<2> ());
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



boost::optional<index_t>
Array::ArrayT::
find_fetch_idx (index_t rand_idx, index_t target_idx)
{
    rand_idx_and_have_idx prog (target_idx, rand_idx);
    stream_process (prog,
		    zero_to_n (*_num_retrievals),
		    &_idxs,
		    NULL);
    
    return prog.have_rand   ?	// the suggested rand_idx is already present in
				// T, so need to try another one.
	none		    :
	( prog.have_target ? Just(rand_idx) : Just(target_idx) );
}




class Array::ArrayT::dummy_fetches_stream_prog
{

    // named indexers into the above arrays
    enum {
	IDX=0,			// the item index
	ITEM=1			// the actual item
    };
	    
public:

    dummy_fetches_stream_prog (index_t target_idx,
			       const optional<pair<size_t,ByteBuffer> > &new_val,
			       size_t elem_size)
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
	    assert (((void)"dummy_fetches_stream_prog should have "
		     "found the target item",
		     _the_item.len() > 0));

	    return _the_item;
	}
    
private:

    index_t _target_idx;
    const optional <pair<size_t, ByteBuffer> > & _new_val;

    size_t _elem_size;

    ByteBuffer _the_item;
};




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

shared_ptr<Array::ArrayT>
Array::ArrayT::
duplicate (index_t new_branch)
{
    return shared_ptr<ArrayT> (new ArrayT (*this, new_branch));
}





//
// class ArrayA
//

Array::ArrayA::ArrayA (const std::string& name, /// the array name
		       const optional<pair<size_t, size_t> >& size_params,
		       const size_t * N,
		       const size_t * elem_size,
		       CryptoProviderFactory *  prov_fact)
    : _name	    (name + DIRSEP + "array"),
      _io	    (_name + "-0", size_params),
      N		    (N),
      _elem_size    (elem_size),
      _prov_fact    (prov_fact)
{
#ifndef NO_ENCRYPT
    // add encrypt/decrypt filter.
    _io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (&_io,
			      shared_ptr<SymWrapper>
			      (new SymWrapper (_prov_fact)))));
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


// make a duplicate of ArrayA b, at this branch index.
Array::ArrayA::ArrayA (const ArrayA& b, unsigned branch)
    : _name	    (b._name),
      _io	    (_name + "-" + itoa(branch),
		     Just (make_pair (b._io.getLen(),
				      b._io.getElemSize()))),
      N		    (b.N),
      _elem_size    (b._elem_size),
      _prov_fact    (b._prov_fact)
{
#ifndef NO_ENCRYPT
    // add encrypt/decrypt filter.
    _io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (&_io,
			      shared_ptr<SymWrapper>
			      (new SymWrapper (_prov_fact)))));
#endif // NO_ENCRYPT
    
    // copy the values across
    stream_process (identity_itemproc<>(),
		    zero_to_n (*N),
		    &b._io,
		    &_io);
}


/// duplicate this ArrayA for a new branch
/// @param branch the new branch index, just given for container name
/// choice
shared_ptr<Array::ArrayA>
Array::ArrayA::duplicate(index_t branch)
{
    return shared_ptr<ArrayA> (new ArrayA (*this, branch));
}


void
Array::ArrayA::repermute (const shared_ptr<TwoWayPermutation>& old_p,
			  const shared_ptr<TwoWayPermutation>& new_p)
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
		shared_ptr<SymWrapper> (new SymWrapper (_prov_fact)))));
#endif

    // copy values across
    stream_process ( identity_itemproc<>(),
		     zero_to_n (*N),
		     &_io,
		     &(*p2_cont_io) );
    
    // run the re-permutation on the new container
    shared_ptr<ForwardPermutation> reperm (new RePermutation (*old_p, *new_p));

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
					  CryptoProviderFactory * crypt_fact,
					  unsigned depth)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name,
					 Just (std::make_pair (len, elem_size)),
					 crypt_fact));

    return insert_arr (arrptr, depth);
}

ArrayHandle::des_t ArrayHandle::newArray (const std::string& name,
					  CryptoProviderFactory * crypt_fact,
					  unsigned depth)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name,
					 none,
					 crypt_fact));

    return insert_arr (arrptr, depth);
}


ArrayHandle::des_t
ArrayHandle::insert_arr (const boost::shared_ptr<Array>& arr,
			 unsigned depth)
{
    des_t desc = _next_array_num++;

    // there should be no entry with idx
//    assert (_arrays.find(idx) == _arrays.end() );
    
    _arrays.insert (make_pair (desc,
			       ArrayHandle (arr, 0, depth, desc))); // branch 0

    return desc;
}    


ArrayHandle
ArrayHandle::getArray (ArrayHandle::des_t desc)
{
    map_t::iterator arr_i = _arrays.find (desc);
    if (arr_i == _arrays.end()) {
	throw illegal_operation_argument (
	    "ArrayHandle::getArray: non-existent array requested");
    }

//    return _arrays[desc];
    return arr_i->second;
}


/// do a select operation, and return the handle of the result.
/// @param sel_first if true select the first array, if false the second one.
ArrayHandle ArrayHandle::select (ArrayHandle& a,
				 ArrayHandle& b,
				 bool sel_first,
				 unsigned depth)
{
    // the two Array objects should be the same. shared_ptr compares pointer
    // values as needed.
    if (a._arr != b._arr)
    {
	throw illegal_operation_argument ("select array on different arrays");
    }
    
    shared_ptr<Array> & arr = a._arr;

    index_t sel_branch = arr->select (a._branch, b._branch, sel_first);

    // toss the two old handles
    _arrays.erase (a._desc);
    _arrays.erase (b._desc);
    
    des_t desc = _next_array_num++;
    ArrayHandle newh (arr, sel_branch, depth, desc);
    _arrays.insert (make_pair (desc, newh));

    return _arrays[desc];
}


ArrayHandle
ArrayHandle::write (index_t idx, size_t off,
		    const ByteBuffer& val,
		    unsigned depth)
    throw (better_exception)
{
    if (depth > _last_depth)
    {
	// new branch!
	index_t new_branch = _arr->newBranch (_branch);

	// add a new Handle with this branch number to the map
	des_t desc = _next_array_num++;
	ArrayHandle newh (_arr, new_branch, depth, desc);
	_arrays.insert (make_pair (desc, newh));

	return _arrays[desc].write(idx, off, val, depth);
    }

    _arr->write (idx, off, _branch, val);
    return *this;
}


/// Read the value from an array index.
/// Encapsulates all the re-fetching and permuting, etc.
/// @param i the index to read
/// @param out the ByteBuffer with the value read out.
/// @param depth the conditional depth of this operation.
/// @return a (potentially new) ArrayHandle which should get subsequent reads to
/// that array branch. a new ArrayHandle is produced if this is a deeper
/// conditional depth.
ArrayHandle
ArrayHandle::read (index_t idx,
		   ByteBuffer & out,
		   unsigned depth)
    throw (better_exception)
{
    if (depth > _last_depth)
    {
	// new branch!
	index_t new_branch = _arr->newBranch (_branch);

	// add a new Handle with this branch number to the map
	des_t desc = _next_array_num++;
	ArrayHandle newh (_arr, new_branch, depth, desc);
	_arrays.insert (make_pair (desc, newh));
	// pass on the read to the new guy
	// use the one stored in _arrays, or the reference to self that he
	// returns will be a temporary.
	return _arrays[desc].read (idx, out, depth);
    }

    out = _arr->read (idx, _branch);
    return *this;
}


CLOSE_NS
