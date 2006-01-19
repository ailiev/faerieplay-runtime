#include <string>
#include <algorithm>		// for swap()

#include <math.h>

#include <boost/shared_ptr.hpp>


#include <pir/common/sym_crypto.h>
#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/io_filter_encrypt.h>

#include "utils.h"
#include "array.h"
#include "batcher-permute.h"




using std::string;
using std::auto_ptr;

using boost::shared_ptr;


// instantiate the static array map
Array::map_t Array::_arrays;
int Array::_next_array_num = 1;


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
      _array_cont	("array-" + name),
      _touched_cont	("touched-" + name),
      
      N			(len),
      _elem_size	(elem_size),
      
      _max_retrievals	(sqrt(float(N)) * lgN_floor(N)),

      _prov_fact	(prov_fact),

      _array_io		(_array_cont, true),
      _touched_io	(_touched_cont, false),

      _rand_prov	(_prov_fact->getRandProvider()),
      
      _p		(auto_ptr<TwoWayPermutation>
                         (new LRPermutation (lgN_ceil(N), 7, _prov_fact))),
      
      _num_retrievals	(0)
{

    // need to:
    // - generate a new permutation,
    // - fill out the container with encrypted zeros (all different because of
    //   IVs)
    // - permute it

    _array_io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (_touched_io,
			      auto_ptr<SymWrapper>
			      (new SymWrapper
			       (_prov_fact)))));

    _touched_io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (_touched_io,
			      auto_ptr<SymWrapper> (
				  new SymWrapper
				  (_prov_fact)))));
    _p->randomize();
}


int Array::newArray (const std::string& name,
		     size_t len, size_t elem_size,
		     CryptoProviderFactory * crypt_fact)
{
    shared_ptr<Array> arrptr (new Array (name, len, elem_size, crypt_fact));

    int idx = _next_array_num++;

    // there should be no entry with idx
    assert (_arrays.find(idx) == _arrays.end() );
    _arrays[idx] = arrptr;

    return idx;
}

    


void Array::write_clear (int idx, const ByteBuffer& val)
    throw (host_exception, comm_exception)
{
    _array_io.write (idx, val);
}

    
ByteBuffer Array::read (int idx)
{
    ByteBuffer buf = do_dummy_fetches (idx, false);

    _num_retrievals++;

    return buf;
}


ByteBuffer Array::do_dummy_fetches (int idx,
				    bool do_writes)
{
    // need to get a random element of T_comp
    int rand_idx = _rand_prov->randint (N - _num_retrievals);
    
    // holding two indices at a time
    int idx1, idx2;

    index_t i;

    // start with the desired index in hand
    idx1 = idx;
    
    for (i = 0; i < _num_retrievals; i++) {
	
	// read in the next index
	idx2 = hostio_read_int (_touched_io, i);

	// adjust the rand_idx one higher if there was a touched index less than
	// it
	if (idx2 <= rand_idx) {
	    rand_idx++;
	}

	// and write out the smaller of the two indices
	if (idx1 > idx2) {
	    std::swap (idx1, idx2);
	}
	hostio_write_int (_touched_io, i, idx1);

	// next...
	idx1 = idx2;
    }

    // and put in the last index
    if (idx1 <= rand_idx) {
	    rand_idx++;
    }
    // note: here i == _num_retrievals
    hostio_write_int (_touched_io, i, idx1);


	
}



void Array::repermute ()
{
    // steps:
    // - generate new permutation
    // - set up re-permute object, with:
    //  - 


    // the new permutation
    LRPermutation p2 (lgN_ceil(N), 7, _prov_fact);
    p2.randomize();

    // a container for the new permuted array.
    // its IOFilterEncrypt will setup the new keys
    shared_ptr<FlatIO> p2_cont_io (new FlatIO (_array_cont + "-p2", false));
    p2_cont_io->appendFilter (auto_ptr<HostIOFilter> (
				  new IOFilterEncrypt (
				      *p2_cont_io,
				      auto_ptr<SymWrapper> (
					  new SymWrapper (
					      _prov_fact)))));
    
    // copy values across
    for (index_t i=0; i < N; i++) {
	ByteBuffer obj;
	_array_io.read (i, obj);
	p2_cont_io->write (i, obj);
    }

    // run the re-permutation on the new container
    auto_ptr<ForwardPermutation> reperm (new RePermutation (*_p, p2));

    Shuffler shuffler  (p2_cont_io, reperm, N);
    shuffler.shuffle ();
    
    // move the new container to _array_cont, removing the old one
    
}
