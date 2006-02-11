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



#ifdef TESTING_ARRAY

#include <fstream>
#include "pir/card/configs.h"


int main (int argc, char *argv[])
{
    // create an array, fill it in with provided data from a text file, then run
    // reads/updates from another text file

    using namespace std;
    
    init_default_configs ();
    g_configs.cryptprov = configs::CryptAny;
    
    do_configs (argc, argv);

    auto_ptr<CryptoProviderFactory> prov_fact = init_crypt (g_configs);

    
#include "array-test-sizes.h"
    
    Array test ("test-array",
		Just (make_pair ((size_t)ARR_ARRAYLEN, (size_t)ARR_OBJSIZE)),
		prov_fact.get());
    
    ifstream cmds ("array-test-cmds.txt");
//    cmds.exceptions (ios::badbit | ios::failbit);
    
    string cmd, idx_s, val;

    unsigned i = 0;

    while (getline (cmds, cmd) &&
	   getline (cmds, idx_s))
    {
	index_t idx = atoi (idx_s.c_str());
	
	cout << endl << "Command " << i++ << ": " << cmd << " [" << idx << "] ";

	if (cmd == "write") {
	    getline (cmds, val);
	    cout << "(" << val << ")";
	    test.write (idx, 0, ByteBuffer (val));
	    cout << " --> ()";
	}
	else if (cmd == "read") {
	    cout << "()";
	    ByteBuffer res = test.read (idx);

	    cout << " --> (";
	    cout.write (res.cdata(), res.len());
	    cout << ")" << endl;
	}
    }

    cout << "Array test run finished @ " << epoch_time << endl;

}


#endif // TESTING_ARRAY


using boost::optional;
using boost::none;


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
	ios[i]->appendFilter (
	    auto_ptr<HostIOFilter>
	    (new IOFilterEncrypt (ios[i],
				  shared_ptr<SymWrapper>
				  (new SymWrapper (_prov_fact)))));
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


//#define NO_REFETCHES


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
    _array_io.flush ();
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
	    index_t T_i = bb2basic<index_t> (objs[IDX]);
	    const ByteBuffer & item = objs[ITEM];

	    //	o_objs[IDX] is not written out
	    
	    if (T_i == _target_idx) {
		assert (_the_item.len() == 0); // we should see 'idx' only once

		_the_item = ByteBuffer (item, ByteBuffer::DEEPCOPY);

		if (_new_val) {
		    // all sizes need to be the same! caller should ensure this
		    assert (_new_val->second.len() == _owner->_elem_size);
		
		    ByteBuffer towrite = _new_val->second;
		    // FIXME: ignoring the offset (new_val->first) for now
		    o_objs[ITEM] = towrite;
		}
	    }

	    else {
		// TIMING:
		if (_new_val) {
		    // will be written out re-encrypted with a new IV
		    o_objs[ITEM] = item;
		}
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
    boost::array<FlatIO*,2> in_ios = { &_workarea.idxs, &_workarea.items };
    boost::array<FlatIO*,2> out_ios = { &_workarea.idxs, NULL };
    
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
    p2_cont_io->appendFilter (
	auto_ptr<HostIOFilter> (
	    new IOFilterEncrypt (
		p2_cont_io.get(),
		shared_ptr<SymWrapper> (new SymWrapper (_prov_fact)))));
    
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


/// add a new distinct element to T (#_touched_io):  either idx or a random
/// index (not already in T).
///
/// working with physical indices here
void Array::append_new_working_item (index_t idx)
    throw (better_exception)
{

    // NOTATION: T is the set of touched indices (stored in _touched_io in
    // sorted order)
    
    // at the end we want:
    // if idx is not in T, it is inserted in the correct place
    // if idx is in T, a random index R \notin T is inserted in the correct
    //    place
    //
    // big question: can we do it in one pass? seems bad, as idx could be large,
    // and R small, so we would not know if idx is in T at the time when R
    // can be inserted.
    //
    // So, two passes: one to see what we need to do, and one to insert the
    // needed element in?
    // 
    // TODO: can comnbine pass two with the actual object fetches.


    // so, pass one, see if idx is in T, and generate R

    // starting value for the random untouched index
    // we'll increment it by the number of elts in T which are smaller than
    // it.
    index_t rand_idx = _rand_prov->randint (N - _num_retrievals);

    index_t T_i;
    bool have_idx = false;
    for (index_t i = 0; i < _num_retrievals; i++) {
	
	T_i = hostio_read_int (_workarea.idxs, i);

	// adjust the rand_idx one higher if this touched index is less than
	// it
	if (T_i <= rand_idx) {
	    rand_idx++;
	}

	if (T_i == idx) {
	    have_idx = true;
	}
    }

    // now, the correct rand_idx is ready, and have_idx is known
    
    index_t to_append = have_idx ? rand_idx : idx;

    // grab the object at to_append and append it and to_append to _workarea

    ByteBuffer obj;
    _array_io.read (to_append, obj);

    hostio_write_int (_workarea.idxs, _num_retrievals, to_append);
    _workarea.items.write (_num_retrievals, obj);

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
