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
      _array_cont	(name + ARRAY_NAME),
//      _workarea_idxs_name	(name + "-touched"),
      
      N			(size_params ? size_params->first  : 0),
      _elem_size	(size_params ? size_params->second : 0),
      
//      _max_retrievals	(lrint (sqrt(float(N))) * lgN_floor(N)),

      _prov_fact	(prov_fact),

      _array_io		(_array_cont, size_params),

      _num_retrievals	(0),

      _rand_prov	(_prov_fact->getRandProvider())

//      _p		(auto_ptr<TwoWayPermutation> (new IdPerm (N))),
{

    // if the array exists, get its sizes, and fill in the size-dependent
    // params.
    if (!size_params) {
	N 	   = _array_io.getLen();
	_elem_size = _array_io.getElemSize();
    }

    _max_retrievals = lrint (sqrt(float(N))) *  lgN_floor(N);
    // this happens for very small N
    if (_max_retrievals >= N) {
	_max_retrievals = N-1;
    }

    // init to identity permutation, and then do repermute()
    _p 		    = auto_ptr<TwoWayPermutation> (new IdPerm (N));

    // set up the working area, with a list of indices and objects
    // can't assign from a temporary unnamed FlatIO for some reason
    FlatIO tmp_idxs (_name + IDXS_NAME,
		     Just(std::make_pair (_max_retrievals, sizeof(size_t))));
    FlatIO tmp_objs (_name + OBJS_NAME,
		     Just(std::make_pair (_max_retrievals, _elem_size)));
    _workarea.idxs = tmp_idxs;
    _workarea.items = tmp_objs;
    

    // add encrypt/decrypt filters to all the IO objects.
    FlatIO * ios [] = { &_array_io, &_workarea.idxs, &_workarea.items };
    for (unsigned i=0; i < ARRLEN(ios); i++) {
#ifndef NO_ENCRYPT
	ios[i]->appendFilter (
	    auto_ptr<HostIOFilter>
	    (new IOFilterEncrypt (ios[i],
				  shared_ptr<SymWrapper>
				  (new SymWrapper (_prov_fact)))));
#endif
    }

    // if array is new, fill out with nulls.
    if (size_params)
    {
	ByteBuffer zero (_elem_size);
	zero.set (0);
	stream_process (make_generate_proc_adapter (make_fconst(zero)),
			zero_to_n (N),
			NULL,
			&_array_io);
    }

    repermute();
}


Array::des_t Array::newArray (const std::string& name,
			      size_t len, size_t elem_size,
			      CryptoProviderFactory * crypt_fact)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name,
					 Just (std::make_pair (len, elem_size)),
					 crypt_fact));

    des_t idx = _next_array_num++;

    // there should be no entry with idx
    assert (_arrays.find(idx) == _arrays.end() );
    
    _arrays[idx] = arrptr;

    return idx;
}


Array & Array::getArray (Array::des_t arr)
{
    map_t::iterator arr_i = _arrays.find (arr);
    if (arr_i == _arrays.end()) {
	throw bad_arg_exception ("Array::getArray: non-existent array requested");
    }

    return *(arr_i->second);
}


void Array::write_clear (index_t idx, const ByteBuffer& val)
    throw (host_exception, comm_exception)
{
    _array_io.write (idx, val);
}




ByteBuffer Array::read (index_t idx)
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
    
    append_new_working_item (p_idx);

    ByteBuffer buf = do_dummy_fetches (p_idx, boost::none);

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
			       Array * owner)
	: _target_idx (target_idx),
	  _new_val (new_val),
	  _owner (owner)
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
		    assert (_new_val->second.len() == _owner->_elem_size);
		
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

    Array * _owner;

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
    boost::array<FlatIO*,2> in_ios =  { &_workarea.idxs, &_workarea.items };
    boost::array<FlatIO*,2> out_ios = { &_workarea.idxs, new_val ? &_workarea.items : NULL };
    
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
    // steps:
    // - generate new permutation
    // - set up re-permute object, with:
    //  - 


#ifndef NO_REFETCHES
    {
	// copy values out of the working area and into the main array

	ByteBuffer item, idxbuf;
	for (index_t i=0; i < _num_retrievals; i++)
	{
	    _workarea.idxs.read  (i, idxbuf);
	    _workarea.items.read (i, item);

	    _array_io.write (bb2basic<index_t> (idxbuf), item);
	}
    }
#endif // NO_REFETCHES
	
    // the new permutation
    auto_ptr<TwoWayPermutation> p2 (new LRPermutation (
					lgN_ceil(N), 7, _prov_fact));
    p2->randomize();

    // a container for the new permuted array.
    // its IOFilterEncrypt will setup the new keys
    shared_ptr<FlatIO> p2_cont_io (new FlatIO (_array_io.getName() + "-p2",
					       std::make_pair (N, _elem_size)));
#ifndef NO_ENCRYPT
    p2_cont_io->appendFilter (
	auto_ptr<HostIOFilter> (
	    new IOFilterEncrypt (
		p2_cont_io.get(),
		shared_ptr<SymWrapper> (new SymWrapper (_prov_fact)))));
#endif

    // copy values across
    stream_process ( identity_itemproc,
		     zero_to_n (N),
		     &_array_io,
		     &(*p2_cont_io) );

    
    // run the re-permutation on the new container
    shared_ptr<ForwardPermutation> reperm (new RePermutation (*_p, *p2));

    Shuffler shuffler  (p2_cont_io, reperm, N);
    shuffler.shuffle ();

//    p2_cont_io->flush();
    
    // install the new parameters
    _array_io = *p2_cont_io;

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


struct insert_new_touched
{
    // this one works on batches of two, one from T.idxs and one from T.items

    enum {
	IDX = 0,
	ITEM = 1
    };
    
    insert_new_touched (index_t new_idx, const ByteBuffer& new_item)
	{
	    inhand.idx	= new_idx;
	    inhand.item = new_item;
	}

    void operator() (const boost::array<index_t,2>& idxs,
		     const boost::array<ByteBuffer,2>& objs,
		     boost::array<ByteBuffer,2>& o_objs)
	{
	    index_t T_i = bb2basic<index_t> (objs[IDX]);

	    assert (((void)"insert_new_touched() should see each T_i only once",
		     T_i != inhand.idx));
	    
	    o_objs[ITEM] = objs[ITEM];

	    // return the smaller-numbered item, keep the bigger one
	    if (T_i > inhand.idx) {
		std::swap (inhand.idx, T_i);
		std::swap (inhand.item, o_objs[ITEM]);
	    }

	    o_objs[IDX] = basic2bb (T_i);
	}

    struct {
	index_t idx;
	ByteBuffer item;
    } inhand;

};


/// add a new distinct element to T (#_workarea.idxs):  either idx or a random
/// index (not already in T).
///
/// working with physical indices here
void Array::append_new_working_item (index_t idx)
    throw (better_exception)
{

    bool have_target;
    bool good_rand = false;
    index_t rand_idx;
    // need to find a random untouched index regardless of whether we need it
    // (ie. target_idx is already touched)
    while (!good_rand)
    {
	rand_idx = _rand_prov->randint (N);
	LOG (Log::PROGRESS, logger,
	     "While accessing physical idx " << idx << ", trying rand_idx " << rand_idx);
	rand_idx_and_have_idx prog (idx, rand_idx);
	stream_process (prog,
			zero_to_n (_num_retrievals),
			&_workarea.idxs,
			NULL);

	good_rand   = !prog.have_rand;
	have_target = prog.have_target;
    }

    index_t to_append = have_target ? rand_idx : idx;

    // grab the object at to_append, and insert it and its index into _workarea
    ByteBuffer obj;
    _array_io.read (to_append, obj);
    {
	insert_new_touched prog (to_append, obj);

	boost::array<FlatIO*,2> in_ios =  { &_workarea.idxs, &_workarea.items };
	boost::array<FlatIO*,2> out_ios = { &_workarea.idxs, &_workarea.items };
	
	stream_process (prog,
			zero_to_n<2> (_num_retrievals),
			in_ios,
			out_ios,
			boost::mpl::size_t<1>(), // batch size
			boost::mpl::size_t<2>()	// number of I/O channels
			);

	// and write in the largest-indexed item at the end of work area.
	_workarea.idxs.write (_num_retrievals, basic2bb (prog.inhand.idx));
	_workarea.items.write (_num_retrievals, prog.inhand.item);
    }
	
    // maintain invariant on the _workarea and _num_retrievals
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
