#include <string>
#include <algorithm>		// for swap()

#include <math.h>

#include <pir/common/sym_crypto.h>
#include <pir/card/io_filter_encrypt.h>

#include "utils.h"
#include "array.h"




using std::string;
using std::auto_ptr;




Array::Array (const string& name,
	      size_t len, size_t elem_size,
              CryptoProviderFactory * crypt_fact)
    : _name		(name),
      _array_cont	("array-" + name),
      _touched_cont	("touched-" + name),
      
      N			(len),
      _elem_size	(elem_size),
      
      _array_io		(_array_cont,
                         auto_ptr<HostIOFilter>
                         (new IOFilterEncrypt (auto_ptr<SymWrapper>
                                               (new SymWrapper (crypt_fact))))),
      _touched_io	(_touched_cont,
                         auto_ptr<HostIOFilter>
                         (new IOFilterEncrypt (auto_ptr<SymWrapper>
                                               (new SymWrapper (crypt_fact))))),

      _rand_prov	(crypt_fact->getRandProvider()),
      
      _p		(auto_ptr<TwoWayPermutation>
                         (new LRPermutation (lgN_ceil(len), 7, crypt_fact))),
      
      _max_retrievals	(sqrt(float(N)) * lgN_floor(N)),
      _num_retrievals	(0)
{
    /*
    init_obj_container (_array_cont,
			N,
			elem_size);

    init_obj_container (_touched_cont,
			_max_retrievals,
			sizeof (unsigned));
    */

    _p->randomize();
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
