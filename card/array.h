// -*- c++ -*-

#include <string>
#include <list>
#include <memory>		// auto_ptr
#include <map>
#include <utility>		// pair

#include <boost/shared_ptr.hpp>
#include <boost/utility.hpp>	// boost::noncopyable
#include <boost/optional/optional.hpp>

#include <pir/card/permutation.h>
#include <pir/card/io.h>
#include <pir/card/io_flat.h>

#ifndef _CARD_ARRAY_H
#define _CARD_ARRAY_H




class Array : boost::noncopyable {

public:

    Array (const std::string& name,
           size_t len, size_t elem_size,
           CryptoProviderFactory * crypt_fact);

    Array ()
	{};
    

    /// a descriptor type for the static Array table
    typedef int des_t;

    /// create a new array, and add it to the internal array map.
    /// @param len number of elements
    /// @param elem_size the maximum size of each element
    /// @return an array descriptor which can be given to getArray() to retrieve
    /// this array.
    static des_t newArray (const std::string& name,
			 size_t len, size_t elem_size,
			 CryptoProviderFactory * crypt_fact)
	throw (better_exception);

    /// get a reference to an array
    /// @param num the array descriptor, from newArray()
    static Array & getArray (des_t num);

    /// Write a value to an array index.
    /// @param idx the target index
    /// @param off the offset in bytes within that index, where we place the new
    /// value
    void write (index_t idx, size_t off,
                const ByteBuffer& val)
        throw (better_exception);

    /// Read the value from an array index.
    /// Encapsulates all the re-fetching and permuting, etc.
    /// @param idx the index
    /// @return new ByteBuffer with the value
    ByteBuffer read (index_t i)
	throw (better_exception);

    /// Write a value non-hidden, probably during initialization
    void write_clear (index_t i, const ByteBuffer& val)
	throw (host_exception, comm_exception);
    
    
private:

    /// re-permute the objects under a new random permutation
    void repermute ();

    /// retrieve or update an object, while doing all the necesary re-fetches.
    /// @param idx the virtual index to read/write
    /// @param new_val pointer to an offset and data value, to be written to
    ///       A[idx] if it is non-null
    /// @return the object at #idx
    ByteBuffer do_dummy_fetches (
	index_t idx,
	boost::optional < std::pair<size_t, ByteBuffer> > new_val)
	throw (better_exception);

    /// add a new distinct element to T (#_touched_io):  either idx or a random
    /// index (not already in T)
    void add_to_touched (index_t idx)
	throw (better_exception);
    

    typedef std::map< int, boost::shared_ptr<Array> > map_t;
    static map_t _arrays;
    static int _next_array_num;
    
    std::string _name		// this array's name
	, _array_cont		// and the container name
	// container of the touched elements. will be kept sorted, and new
	// arrivals inserted into it.
	, _touched_cont;
    
    size_t N
	,  _elem_size;

    size_t		_max_retrievals;
    
    // need to hand a factory to permutation objects many times, so hang onto
    // one
    CryptoProviderFactory * _prov_fact;


    /// The encrypted and permuted items
    FlatIO _array_io;

    /// A sorted list of encrypted physical indices which have been touched in
    /// previous retrievals.
    ///
    /// \invariant len (#_touched_io) = #_num_retrievals
    FlatIO _touched_io;

    /// how many retrievals already done this session?
    size_t              _num_retrievals;

    std::auto_ptr<RandProvider> _rand_prov;

/* TODO: do this next...
    // how many branches deep is this array now?
    int 	_depth;         // _depth = _divergences.size()

    // this is more like a stack
    list<divergence_point> _divergences;
*/
    
    // PIRW algorithm parameters

    std::auto_ptr<TwoWayPermutation>   _p;
    
    /// batchsize for index's
    size_t _idx_batchsize;
    /// batchsize for objects
    size_t _obj_batchsize;
};



#endif // _CARD_ARRAY_H
