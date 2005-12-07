// -*- c++ -*-

#include <string>
#include <list>
#include <memory>

#include <pir/card/permutation.h>
#include <pir/card/io.h>

#ifndef _CARD_ARRAY_H
#define _CARD_ARRAY_H

class Array {

public:

    Array (const std::string& name,
           size_t len, size_t elem_size,
           CryptoProviderFactory * crypt_fact);

    /// Write a value to an array index
    /// @param idx the target index
    /// @param off the offset in bytes within that index, where we place the new
    /// value
    void write (int idx, int off,
                const ByteBuffer& val)
        throw (host_exception, comm_exception);

    /// Read the value from an array index.
    /// Encapsulates all the re-fetching and permuting, etc.
    /// @param idx the index
    /// @return new ByteBuffer with the value
    ByteBuffer read (int idx);

    /// Write a value non-hidden, probably during initialization
    void write_clear (int idx, const ByteBuffer& val)
	throw (host_exception, comm_exception);
    
    
private:

    void repermute ();

    ByteBuffer do_dummy_fetches (int idx,
				 bool do_writes);
    
    std::string _name		// this array's name
	, _array_cont		// and the container name
	// container of the touched elements. will be kept sorted, and new
	// arrivals inserted into it.
	, _touched_cont;
    
    size_t N
	,  _elem_size;

//    SymWrapper _sym_wrapper; held by the _io

    HostIO _array_io, _touched_io;

    std::auto_ptr<RandProvider> _rand_prov;

/* TODO: do this next...
    // how many branches deep is this array now?
    int 	_depth;         // _depth = _divergences.size()

    // this is more like a stack
    list<divergence_point> _divergences;
*/
    
    // PIRW algorithm parameters

    std::auto_ptr<TwoWayPermutation>   _p;
    
    size_t		_max_retrievals;
    
    // how many retrievals this session?
    size_t              _num_retrievals;
};



#endif // _CARD_ARRAY_H
