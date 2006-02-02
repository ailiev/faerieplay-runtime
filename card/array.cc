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


using namespace std;

int main (int argc, char *argv[])
{
    // create an array, fill it in with provided data from a text file, then run
    // reads/updates from another text file

    init_default_configs ();
    g_configs.cryptprov = configs::CryptAny;
    
    do_configs (argc, argv);

    auto_ptr<CryptoProviderFactory> prov_fact = init_crypt (g_configs);

    
#include "array-test-sizes.h"
    
    Array test ("test-array",
		Just (std::make_pair ((size_t)ARR_ARRAYLEN, (size_t)ARR_OBJSIZE)),
		prov_fact.get());
    
    ifstream cmds ("array-test-cmds.txt");
//    cmds.exceptions (ios::badbit | ios::failbit);
    
    string cmd, idx_s, val;

    unsigned i = 0;

    while (getline (cmds, cmd) &&
	   getline (cmds, idx_s))
    {
	index_t idx = atoi (idx_s.c_str());
	
	cout << endl << "Command " << i++ << ": " << cmd << " " << idx;

	if (cmd == "write") {
	    getline (cmds, val);
	    cout << "(" << val << ")";
	    test.write (idx, 0, ByteBuffer (val));
	}
	else if (cmd == "read") {
	    ByteBuffer res = test.read (idx);

	    cout << " --> ";
	    cout.write (res.cdata(), res.len());
	    cout << endl;
	}
    }

    clog << "Array test run finished @ " << epoch_time << endl;

}


#endif // TESTING_ARRAY


using boost::optional;
using boost::none;


namespace {
    unsigned s_log_id = Log::add_module ("array");
}



// instantiate the static array map
Array::map_t Array::_arrays;
Array::des_t Array::_next_array_num = 1;


void prefetch_contiguous_from (FlatIO & io,
			       index_t start,
			       size_t howmany)
    throw (better_exception)
{
    return;
    FlatIO::idx_list_t l (howmany);
    std::generate (l.begin(), l.end(), counter(start));
    io.prefetch (l);
}


namespace {
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

      _array_io		(_array_cont,
			 size_params ? Just(N) : none),

//      _touched_io	(_touched_cont, _max_retrievals),
      _num_retrievals	(0),

      _rand_prov	(_prov_fact->getRandProvider()),

//      _p		(auto_ptr<TwoWayPermutation> (new IdPerm (N))),
      _idx_batchsize	(64),

      // FIXME: should always be able to use the element size here
      _obj_batchsize	(size_params		    ?
			 (16*(1<<10)) / _elem_size  : // use 16KB total
			 32)
{

    // if the array exists, get its length, and fill in the length-dependent
    // params.
    if (!size_params) {
	N = _array_io.getLen();
	// TODO: _elem_size ...
    }

    _max_retrievals = lrint (sqrt(float(N))) *  lgN_floor(N);

    // can't assign to an unnamed FlatIO for some reason
    FlatIO tmp_idxs (_name + IDXS_NAME, Just(_max_retrievals));
    FlatIO tmp_objs (_name + OBJS_NAME, Just(_max_retrievals));
    _workarea.idxs = tmp_idxs;
    _workarea.items = tmp_objs;
    
    // init to identity permutation, and then do repermute()
    _p 		    = auto_ptr<TwoWayPermutation> (new IdPerm (N));


    // need to:
    // - generate a new permutation,
    // - fill out the container with encrypted zeros (all different because of
    //   IVs)
    // - permute it

    FlatIO * ios [] = { &_array_io, &_workarea.idxs, &_workarea.items };
    for (unsigned i=0; i < ARRLEN(ios); i++) {
	ios[i]->appendFilter (
	    auto_ptr<HostIOFilter>
	    (new IOFilterEncrypt (ios[i],
				  shared_ptr<SymWrapper>
				  (new SymWrapper (_prov_fact)))));
    }

    // fill out a new array with nulls
    if (size_params) {
	ByteBuffer zero (_elem_size);
	zero.set (0);
	for (index_t i=0; i < N; i++) {
	    write_clear (i, zero);
	}
	_array_io.flush();
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

//     ByteBuffer obj;
//     _array_io.read (p_idx, obj);
//     _num_retrievals++;
//     if (_num_retrievals > _max_retrievals) {
// 	repermute ();
//     }
//     return obj;
    
    
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

//     _array_io.write (p_idx, val);
//     _array_io.flush ();
//     _num_retrievals++;
//     if (_num_retrievals > _max_retrievals) {
// 	repermute ();
//     }
//     return;
    
    append_new_working_item (p_idx);

    
    do_dummy_fetches (p_idx,
		      std::make_pair (off, val));

    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
}



/// retrieve or update an object, while doing all the necesary re-fetches.
/// @param idx the physical index to read/write
/// @param new_val pointer to an offset and data value, to be written to
///       A[idx] if it is non-null
/// @return the object at #idx
ByteBuffer Array::do_dummy_fetches (index_t idx,
				    optional < pair<size_t, ByteBuffer> > new_val)
    throw (better_exception)
{
    ByteBuffer obj, the_obj;
    
    // TODO: if we are reading, no need to decrypt all the objects, but just the
    // real one at the end.
    
    unsigned that_idx = 0;

    // go through all the _workarea indices and objects. if we have new_val,
    // re-write every item (not index); otherwise just read and keep the one
    // numbered idx, which should be there after append_new_working_item()
    for (index_t i = 0; i < _num_retrievals; i++) {

	that_idx = hostio_read_int (_workarea.idxs, i);
	_workarea.items.read (i, obj);

	if (that_idx == idx) {
	    the_obj = ByteBuffer (obj, ByteBuffer::DEEPCOPY);
	    if (new_val) {
		// all sizes need to be the same! caller should ensure this
		assert (new_val->second.len() == _elem_size);
		
		ByteBuffer towrite = new_val->second;
		// FIXME: ignoring the offset (new_val->first) for now
		_workarea.items.write (i, towrite);
	    }
	}

	else {
	    // TIMING:
	    if (new_val) {
		// written out re-encrypted with a new IV
		_workarea.items.write (i, obj);
	    }
	}

    }

    _workarea.idxs.flush ();
    _workarea.items.flush ();

    // we must have read the object in during the above loop
    assert (the_obj.len() > 0);

    return the_obj;
}



void Array::repermute ()
{
    // steps:
    // - generate new permutation
    // - set up re-permute object, with:
    //  - 


    {
	// copy values out of the working area and into the main array
	index_t idx;
	ByteBuffer obj;
	for (index_t i=0; i < _num_retrievals; i++)
	{
	    idx = hostio_read_int (_workarea.idxs, i);
	    _workarea.items.read (i, obj);

	    _array_io.write (idx, obj);
	}

	_array_io.flush();
    }
	
    // the new permutation
    auto_ptr<TwoWayPermutation> p2 (new LRPermutation (
					lgN_ceil(N), 7, _prov_fact));
    p2->randomize();

    // a container for the new permuted array.
    // its IOFilterEncrypt will setup the new keys
    shared_ptr<FlatIO> p2_cont_io (new FlatIO (_array_io.getName() + "-p2", N));
    p2_cont_io->appendFilter (
	auto_ptr<HostIOFilter> (
	    new IOFilterEncrypt (
		p2_cont_io.get(),
		shared_ptr<SymWrapper> (new SymWrapper (_prov_fact)))));
    
    // copy values across
//    _array_io.flush ();
    {
	ByteBuffer obj;
	for (index_t i=0; i < N; i++) {
	    _array_io.read (i, obj);
	    p2_cont_io->write (i, obj);
	}
	p2_cont_io->flush();
    }
    
    // run the re-permutation on the new container
    shared_ptr<ForwardPermutation> reperm (new RePermutation (*_p, *p2));

    Shuffler shuffler  (p2_cont_io, reperm, N);
    shuffler.shuffle ();

    p2_cont_io->flush();
    
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
	
	if (i % _idx_batchsize == 0) {
	    prefetch_contiguous_from (
		_workarea.idxs,
		i,
		std::min (_idx_batchsize, _num_retrievals - i));
	}

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
    _workarea.idxs.flush();
    _workarea.items.write (_num_retrievals, obj);
    _workarea.items.flush();

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
