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
           const boost::optional <std::pair<size_t, size_t> >& size_params,
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
    

    class dummy_fetches_stream_prog;
    friend class dummy_fetches_stream_prog;
    
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

    /// append a new distinct index (+ object at that index) to W (_workarea):
    /// either idx or a random index (not already in W)
    void append_new_working_item (index_t idx)
	throw (better_exception);

    void standard_prefetch_touched (index_t i,
				    bool do_idxs, bool do_objs)
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

    struct WorkArea {
	FlatIO idxs, items;
    };
    
    /// A sorted list of encrypted physical indices along with the actual items,
    /// which have been touched in previous retrievals.
    ///
    /// \invariant len (#_touched_io) = #_num_retrievals
    WorkArea _workarea;

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

#if 0
/// The object to be used by users for array operations.
/// handles depth - forking new array forks when we go deeper, and dealing with
/// the multiple T sets in a forked array.
class ArrayHandle {

public:

    ArrayHandle (boost::shared_ptr<Array> arr,
		 index_t t_index)
	: _arr (arr),
	  _T_index (t_index)
	{}

    // the three methods just proxy on to the Array object

    /// Write a value to an array index.
    /// @param idx the target index
    /// @param off the offset in bytes within that index, where we place the new
    /// value
    void write (unsigned depth,
		index_t idx, size_t off,
                const ByteBuffer& val)
        throw (better_exception)
	{
	    _arr->write (_T_index, idx, off, val);
	}

    /// Read the value from an array index.
    /// Encapsulates all the re-fetching and permuting, etc.
    /// @param idx the index
    /// @return new ByteBuffer with the value
    ByteBuffer read (unsigned depth, index_t i)
	throw (better_exception)
	{
	    return _arr->read (i);
	}

    /// Write a value non-hidden, probably during initialization
    void write_clear (index_t i, const ByteBuffer& val)
	throw (host_exception, comm_exception)
	{
	    _arr->write_clear (_T_index, i, val);
	}
    
private:
    // the array
    boost::shared_ptr<Array> _arr;
    // which is this array fork's T?
    index_t _T_index;

    unsigned _last_depth;
};
#endif



#endif // _CARD_ARRAY_H
