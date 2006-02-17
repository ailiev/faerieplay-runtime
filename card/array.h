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
    


    /// Write a value to an array index.
    /// @param idx the target index
    /// @param off the offset in bytes within that index, where we place the new
    /// value
    void write (index_t idx, size_t off,
		index_t branch,
                const ByteBuffer& val)
        throw (better_exception);

    /// Read the value from an array index.
    /// Encapsulates all the re-fetching and permuting, etc.
    /// @param idx the index
    /// @return new ByteBuffer with the value
    ByteBuffer read (index_t i,
		     index_t branch)
	throw (better_exception);

    // make a new branch and return its index
    index_t newBranch (index_t source_branch);
    
    /// Write a value non-hidden, probably during initialization
//     void write_clear (index_t i, const ByteBuffer& val)
// 	throw (host_exception, comm_exception);
    

    
private:

    /// re-permute the objects under a new random permutation
    void repermute ();


    class ArrayT
    {
    public:
	// used when creating the first T for an unbranched array.
	ArrayT (const std::string& name, /// the array name
		size_t max_retrievals,
		size_t elem_size);

	/// @return the index to fetch into T. none if rand_idx is already in
	/// this T.
	boost::optional<index_t>
	find_fetch_idx (index_t rand_idx, index_t target_idx);

	/// Read (and maybe write) all the elements in this T, returning the
	/// current value of target_index
	/// PRE: the target_index must be in this T
	ByteBuffer do_dummy_accesses (index_t target_index,
				      < std::pair<size_t, ByteBuffer> > new_val);

	void appendItem (index_t idx, const ByteBuffer& item);

	// called when a new branch is to be created.
	// new_branch is just provided to enable useful naming of the container.
	ArrayT duplicate (index_t new_branch);

	class dummy_fetches_stream_prog;
	friend class dummy_fetches_stream_prog;
	
    private:

	// used by duplicate()
	ArrayT (const std::string& name, /// the array name
		unsigned branch_num, // this will be the b-th branch
		size_t elem_size);


	
	std::string _cont_name;
	index_t _branch;	// our branch number

	FlatIO _idxs, _items;

	/// how many retrievals already done this session?
	/// this should be the same as the owner Array's _num_retrievals, but
	/// would be tedious to pass it to every method here.
	size_t              _num_retrievals;
    };

    

    class ArrayA
    {
    public:
	
	ArrayA (const std::string& name, /// the array name
		size_t elem_size);

	void
	repermute (const boost::shared_ptr<TwoWayPermutation>& old_p,
		   const boost::shared_ptr<TwoWayPermutation>& new_p);

	ArrayA duplicate(index_t branch);

	friend class Array;
	
    private:
	// used by duplicate()
	ArrayT (const std::string& name, /// the array name
		unsigned branch_num, // this is the b-th branch (for naming
				     // purposes)
		size_t elem_size);

	FlatIO _io;
    };


    // the selection routines
    static ArrayT
    select (const ArrayT& a, const ArrayT& b, bool which);

    static ArrayA
    select (const ArrayA& a, const ArrayA& b, bool which);

    /// merge a T into an A
    static void
    merge (const ArrayT& t, ArrayA& a);
	
	
    // get tha A for this branch
    shared_ptr<ArrayA> & getA (index_t branch);
    
    bool have_mult_As();

    void repermute_As();
    
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
    

    //
    // variables
    //
    
    std::vector<boost::shared_ptr<ArrayT> > _Ts;
    // most of the time we'll have just one A, so to simplify:
//     boost::variant <boost::shared_ptr<ArrayA>,
// 		    std::vector<boost::shared_ptr<ArrayA> > > _As;
    // actually much simpler for me like this!
    std::vector<boost::shared_ptr<ArrayA> > _As;
    
    size_t _num_branches;

    std::string _name;		// this array's name

    
    size_t N
	,  _elem_size;

    // need to hand a factory to permutation objects many times, so hang onto
    // one
    CryptoProviderFactory * _prov_fact;


    std::auto_ptr<RandProvider> _rand_prov;

    // PIRW algorithm parameters

    std::auto_ptr<TwoWayPermutation>   _p;

    static Log::logger_t _logger;

    DECL_STATIC_INIT( Array::_logger = Log::makeLogger ("array",
							none, none); );
    
};


DECL_STATIC_INIT_INSTANCE (Array);


#if 0
/// The object to be used by users for array operations.
/// handles depth - forking new array forks when we go deeper, and dealing with
/// the multiple T sets in a forked array.
class ArrayHandle {

public:

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
    static ArrayHandle & getArray (des_t num);


    // do a select operation, and return the handle of the result.
    static des_t select (const ArrayHandle& a,
			 const ArrayHandle& b,
			 bool which);

    
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

    // used in newArray
    ArrayHandle (boost::shared_ptr<Array> arr,
		 index_t t_index)
	: _arr (arr),
	  _T_index (t_index)
	{}

    boost::shared_ptr<Array> _arr;
    
    index_t _branch_index;

    unsigned _last_depth;

    // the array map
    typedef std::map< int, boost::shared_ptr<ArrayHandle> > map_t;
    static map_t _arrays;
    static int _next_array_num;

};
#endif // 0



#endif // _CARD_ARRAY_H
