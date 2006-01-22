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

#include <pir/common/openssl_crypto.h>

using namespace std;

int main (int argc, char *argv[])
{
    // create an array, fill it in with provided data from a text file, then run
    // reads/updates from another text file

    cout << "Start array test run @ " << epoch_time << endl;

    CryptoProviderFactory * prov_fact = new OpenSSLCryptProvFactory ();
    
    Array test ("test-array",
		256, 10, prov_fact);
    
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

    cout << "Array test run finished @ " << epoch_time << endl;

}


#endif // TESTING_ARRAY




// instantiate the static array map
Array::map_t Array::_arrays;
Array::des_t Array::_next_array_num = 1;


// Containers used:
// 
// array: the encrypted but unpermuted array values. may or may not exists when
// the Array object is instantiated:
// - exists if previusly created eg. as part of input processing.
// - does not exist if eg. doing an InitDynArray gate now
//
// touched: the elements accessed this session, encrypted and kept sorted (by
// us)
// does not exist when the Array object is created.

Array::Array (const string& name,
	      size_t len, size_t elem_size,
              CryptoProviderFactory * prov_fact)
    : _name		(name),
      _array_cont	(name + "-array"),
      _touched_cont	(name + "-touched"),
      
      N			(len),
      _elem_size	(elem_size),
      
      _max_retrievals	(lrint (sqrt(float(N))) * lgN_floor(N)),

      _prov_fact	(prov_fact),

      _array_io		(_array_cont, false),

      _touched_io	(_touched_cont, false),
      _num_retrievals	(0),

      _rand_prov	(_prov_fact->getRandProvider()),

      // init to identity permutation, and then do repermute()
      _p		(auto_ptr<TwoWayPermutation> (new IdPerm (N)))
{

    // need to:
    // - generate a new permutation,
    // - fill out the container with encrypted zeros (all different because of
    //   IVs)
    // - permute it

    _array_io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (&_array_io,
			      shared_ptr<SymWrapper>
			      (new SymWrapper
			       (_prov_fact)))));

    _touched_io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (&_touched_io,
			      shared_ptr<SymWrapper> (
				  new SymWrapper
				  (_prov_fact)))));
    ByteBuffer zero (_elem_size);
    zero.set (0);
    for (index_t i=0; i < N; i++) {
	write_clear (i, zero);
    }

    repermute();
}


Array::des_t Array::newArray (const std::string& name,
			      size_t len, size_t elem_size,
			      CryptoProviderFactory * crypt_fact)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name, len, elem_size, crypt_fact));

    des_t idx = _next_array_num++;

    // there should be no entry with idx
    assert (_arrays.find(idx) == _arrays.end() );
    _arrays[idx] = arrptr;

    return idx;
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


    add_to_touched (idx);

    ByteBuffer buf = do_dummy_fetches (idx, boost::none);

    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }

    return buf;
}

void Array::write (index_t idx, size_t off,
		   const ByteBuffer& val)
    throw (better_exception)
{
    
    add_to_touched (idx);

    
    do_dummy_fetches (idx,
		      std::make_pair (off, val));

    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
}



ByteBuffer Array::do_dummy_fetches (index_t idx,
				    optional < pair<size_t, ByteBuffer> > new_val)
    throw (better_exception)
{
    ByteBuffer obj, the_obj;
    
    // TODO: if we are reading, no need to decrypt all the objects, but just the
    // real one at the end.
    
    for (index_t i = 0; i < _num_retrievals; i++) {

	_array_io.read (idx, obj);

	if (i == idx) {
	    the_obj = obj;
	    if (new_val) {
		// FIXME: all sizes need to be the same!

		assert (new_val->second.len() <= _elem_size);
		
		ByteBuffer towrite = new_val.get().second;
		if (new_val->second.len() < _elem_size) {
		    towrite = realloc_buf (towrite, _elem_size);
		}
		// FIXME: ignoring the offset for now
		_array_io.write (idx, towrite);
	    }
	}

	else {
	    if (new_val) {
		// written out re-encrypted with a new IV
		_array_io.write (idx, obj);
	    }
	}

    }

    return the_obj;
}



void Array::repermute ()
{
    // steps:
    // - generate new permutation
    // - set up re-permute object, with:
    //  - 


    // the new permutation
    auto_ptr<TwoWayPermutation> p2 (new LRPermutation (
					lgN_ceil(N), 7, _prov_fact));
    p2->randomize();

    // a container for the new permuted array.
    // its IOFilterEncrypt will setup the new keys
    shared_ptr<FlatIO> p2_cont_io (new FlatIO (_array_cont + "-p2", false));
    p2_cont_io->appendFilter (auto_ptr<HostIOFilter> (
				  new IOFilterEncrypt (
				      p2_cont_io.get(),
				      shared_ptr<SymWrapper> (
					  new SymWrapper (
					      _prov_fact)))));
    
    // copy values across
    for (index_t i=0; i < N; i++) {
	ByteBuffer obj;
	_array_io.read (i, obj);
	p2_cont_io->write (i, obj);
    }

    // run the re-permutation on the new container
    shared_ptr<ForwardPermutation> reperm (new RePermutation (*_p, *p2));

    Shuffler shuffler  (p2_cont_io, reperm, N);
    shuffler.shuffle ();
    
    // install the new parameters
    _array_io = *p2_cont_io;

    _p = p2;

    _num_retrievals = 0;
}


// private
void Array::add_to_touched (index_t idx)
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
	
	T_i = hostio_read_int (_touched_io, i);

	// adjust the rand_idx one higher if this touched index is less than
	// it
	if (T_i <= rand_idx) {
	    rand_idx++;
	}

	if (T_i == idx) {
	    have_idx = true;
	}
    }

    // and now do the insertion in T
    index_t to_insert = have_idx ? rand_idx : idx;
    
    // holding one value at a time, and reading another one in from the list
    index_t inhand, read_in;

    // start with the new value in hand
    inhand = to_insert;
    
    for (index_t i = 0; i < _num_retrievals; i++) {
	
	// read in the next value
	read_in = hostio_read_int (_touched_io, i);

	// write out the smaller of the two values
	hostio_write_int (_touched_io, i, min (inhand, read_in));

	// and keep the bigger for next loop
	inhand = max (inhand, read_in);
    }

    // the largest element
    hostio_write_int (_touched_io, _num_retrievals, inhand);

    // maintain invariant on the _touched_io and _num_retrievals
    _num_retrievals++;
}
