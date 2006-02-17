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
		size_t elem_size,
		const size_t * p_num_retrievals);

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
	/// make this point into the owner Array's _num_retrievals, who will
	/// then be responsible for updating it.
	const size_t * _num_retrievals;
    };

    

    class ArrayA
    {
    public:
	
	ArrayA (const std::string& name, /// the array name
		const boost::optional<std::pair<size_t, size_t> >& size_params);

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

	// again these are set up to point into the owner Array object
	const size_t *N, *_elem_size;
    };


    // the selection routines
    static ArrayT
    select (const ArrayT& a, const ArrayT& b, bool which);

    static ArrayA
    select (const ArrayA& a, const ArrayA& b, bool which);

    /// merge a T into an A
    static void
    merge (const ArrayT& t, ArrayA& a);
	

    
    bool have_mult_As();

    void repermute_As();

    /// append a new distinct index (+ object at that index) to W (_workarea):
    /// either idx or a random index (not already in W)
    void append_new_working_item (index_t idx)
	throw (better_exception);

    

    //
    // variables
    //
    

    std::string _name;		// this array's name

    
    size_t N
	,  _elem_size;

    // need to hand a factory to permutation objects many times, so hang onto
    // one
    CryptoProviderFactory * _prov_fact;


    std::auto_ptr<RandProvider> _rand_prov;

    // PIRW algorithm parameters

    std::auto_ptr<TwoWayPermutation>   _p;

    std::vector<boost::shared_ptr<ArrayT> > _Ts;
    // most of the time we'll have just one A, so to simplify:
//     boost::variant <boost::shared_ptr<ArrayA>,
// 		    std::vector<boost::shared_ptr<ArrayA> > > _As;
    // actually much simpler for me like this!
    std::vector<boost::shared_ptr<ArrayA> > _As;
    
    size_t _num_branches;

    size_t _num_retrievals;

    static Log::logger_t _logger;

    DECL_STATIC_INIT( Array::_logger = Log::makeLogger ("array",
							none, none); );
    
};


DECL_STATIC_INIT_INSTANCE (Array);


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

    /// in the case of an exisitng array, don;t provide any length params, but
    /// read them (and the atrray) of the host
    static des_t newArray (const std::string& name,
			   CryptoProviderFactory * crypt_fact)
	throw (better_exception);


    /// get a reference to an array
    /// @param num the array descriptor, from newArray()
    static ArrayHandle & getArray (des_t num);


    // do a select operation, and return the handle of the result.
    static des_t select (const ArrayHandle& a,
			 const ArrayHandle& b,
			 bool which);

    /// return this ArrayHandle's descriptor
    des_t getDescriptor()
	{
	    return _desc;
	}

    
    // the three methods just proxy on to the Array object, adding a branch
    // parameter.

    /// Write a value to an array index.
    /// @param idx the target index
    /// @param off the offset in bytes within that index, where we place the new
    /// value
    void write (unsigned depth,
		index_t idx, size_t off,
                const ByteBuffer& val)
    throw (better_exception);

    /// Read the value from an array index.
    /// Encapsulates all the re-fetching and permuting, etc.
    /// @param idx the index
    /// @return new ByteBuffer with the value
    ByteBuffer read (unsigned depth, index_t i)
	throw (better_exception);

    /// Write a value non-hidden, probably during initialization.
    /// FIXME: do we need to consider branches here?
    void write_clear (index_t i, const ByteBuffer& val)
	throw (host_exception, comm_exception)
	{
	    _arr->write_clear (i, val);
	}

private:

    // used in newArray
    ArrayHandle (boost::shared_ptr<Array> arr,
		 index_t branch,
		 unsigned depth,
		 des_t desc)
	: _arr (arr),
	  _branch (branch),
	  _last_depth (depth),
	  _desc (desc),
	{}

    static des_t insert_arr (const boost::shared_ptr<Array>& arr);

    boost::shared_ptr<Array> _arr;
    
    index_t _branch;

    unsigned _last_depth;

    des_t _desc;

    //
    // static
    //
    // the array map
    typedef std::map<int, ArrayHandle> map_t;
    static map_t _arrays;
    static int _next_array_num;

};



#endif // _CARD_ARRAY_H
