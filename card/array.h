// -*- c++ -*-
/*
 * Circuit virtual machine for the Faerieplay hardware-assisted secure
 * computation project at Dartmouth College.
 *
 * Copyright (C) 2003-2007, Alexander Iliev <alex.iliev@gmail.com> and
 * Sean W. Smith <sws@cs.dartmouth.edu>
 *
 * All rights reserved.
 *
 * This code is released under a BSD license.
 * Please see LICENSE.txt for the full license and disclaimers.
 *
 */

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



OPEN_NS


class Array : boost::noncopyable {

public:

    /// create an array.
    /// This constructor does not do an initial permutation! user should call
    /// repermute() when needed.
    /// @param size_params the length and element size of the new array. none if
    /// the array should be on disk already.
    Array (const std::string& name,
           const boost::optional <std::pair<size_t, size_t> >& size_params,
           CryptoProviderFactory * crypt_fact);

    Array ()
	{};
    


    /// Write a value to an array index.
    /// @param idx the target index
    /// @param off the offset in bytes within that index, where we place the new
    /// value
    void write (bool enable, index_t idx, size_t off,
                const ByteBuffer& val)
        throw (better_exception);

    /// Read the value from an array index.
    /// Encapsulates all the re-fetching and permuting, etc.
    /// @param idx the index
    /// @return new ByteBuffer with the value
    ByteBuffer read (index_t i)
	throw (better_exception);

    
    /// Write a value non-hidden, probably during initialization
    void write_clear (index_t i, size_t off, const ByteBuffer& val)
 	throw (host_exception, comm_exception);

    /// Read a value without index protection and with minimal effort.
    ByteBuffer read_clear (index_t i)
	throw (better_exception);
    
    
    /// re-permute the objects under a new random permutation
    void repermute ();

    /// get this array's length
    size_t length () const
	{
	    return N;
	}

    size_t elem_size () const
	{
	    return _elem_size;
	}

    const std::string& name () const
	{
	    return _name;
	}

    // almost like an operator<<
    static std::ostream& print (std::ostream& os,
				Array & arr);
    

    
private:

    class ArrayA
    {
    public:
	
	ArrayA (const std::string& name, /// the array name
		const boost::optional<std::pair<size_t, size_t> >& size_params,
		const index_t* N,
		const index_t* elem_size,
		CryptoProviderFactory *  prov_fact);

	void
	repermute (const boost::shared_ptr<TwoWayPermutation>& old_p,
		   const boost::shared_ptr<TwoWayPermutation>& new_p);

	friend class Array;
	
    private:
	// used by duplicate()
	ArrayA (const ArrayA& b); // the source ArrayA


	std::string _name;	// the array name
	
	mutable FlatIO _io;

	// again these are set up to point into the owner Array object
	const size_t *N, *_elem_size;

	CryptoProviderFactory * _prov_fact;

    };


    
    class ArrayT
    {
    public:
	ArrayT (const std::string& name, /// the array name
		size_t max_retrievals,
		size_t elem_size,
		const size_t * p_num_retrievals,
		CryptoProviderFactory * prov_fact);

	/// @return the index to fetch into T. none if rand_idx is already in
	/// this T.
	boost::optional<index_t>
	find_fetch_idx (index_t rand_idx, index_t target_idx);

	/// Read (and maybe write) all the elements in this T, returning the
	/// current value of target_index
	/// PRE: the target_index must be in this T
	ByteBuffer do_dummy_accesses (
	    index_t target_index,
	    const boost::optional <std::pair<size_t, ByteBuffer> > & new_val);

	void appendItem (index_t idx, const ByteBuffer& item);


	class dummy_fetches_stream_prog;
	friend class dummy_fetches_stream_prog;
	
	friend class Array;
	
	
    private:

	std::string _name;

	mutable FlatIO
	_idxs,			// this is the physical (ie. permuted) index
				// of the item
	    _items;		// the actual item.

	/// how many retrievals already done this session?
	/// make this point into the owner Array's _num_retrievals, who will
	/// then be responsible for updating it.
	const size_t * _num_retrievals;

	CryptoProviderFactory * _prov_fact;

    };

    

    /// merge a T into an A
    static void
    merge (const ArrayT& t, ArrayA& a);
	

    /// append a new distinct object (and its index) to T.
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


    // the current permutation. kept as a shared_ptr mostly because that's what
    // Shuffler expects.
    boost::shared_ptr<TwoWayPermutation>   _p;

    std::auto_ptr<ArrayT> _T;
    std::auto_ptr<ArrayA> _A;
    
    size_t _max_retrievals, _num_retrievals;


public:
    static Log::logger_t _logger;
    
    DECL_STATIC_INIT( Array::_logger = Log::makeLogger (
			  "circuit-vm.card.array");
	);
    
};


DECL_STATIC_INIT_INSTANCE (Array);








/// The object to be used by users for array operations.
/// handles depth - forking new array forks when we go deeper, and dealing with
/// the multiple T sets in a forked array.
class ArrayHandle {

public:

    /// a descriptor type for the static Array table
    typedef int des_t;

    /// default constr. required for std::map::operator[]. produces an unusable
    /// object.
    ArrayHandle ()
	{}
    
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


    /// return a reference to this ArrayHandle's descriptor
    const des_t & getDescriptor() const
	{
	    return _desc;
	}

    
    //
    // the three methods just proxy on to the Array object
    //
    
    /// Write a value to an array index.
    /// @param idx the target index
    /// @param off the offset in bytes within that index, where we place the new
    /// value
    ArrayHandle &
    write (bool enable,
	   boost::optional<index_t> idx, size_t off,
	   const ByteBuffer& val)
	throw (better_exception);

    /// Read the value from an array index.
    /// Encapsulates all the re-fetching and permuting, etc.
    /// @param idx the index
    /// @param out The value read out of the array.
    /// @return handle to (potentially new) Array.
    ArrayHandle &
    read (bool enable, boost::optional<index_t> i, ByteBuffer & out)
	throw (better_exception);
    
    /// Write a value non-hidden, probably during initialization.
    void write_clear (index_t i, const ByteBuffer& val)
	throw (host_exception, comm_exception)
	{
	    _arr->write_clear (i, 0, val);
	}

    ByteBuffer read_clear (index_t i)
	throw (better_exception)
	{
	    return _arr->read_clear (i);
	}

    size_t length () const
	{
	    return _arr->length();
	}


    // print an ArrayHandle's contents, in order from index 0 to N-1
    friend std::ostream& operator<< (std::ostream& os, ArrayHandle& arr);

private:

    // used in newArray
    ArrayHandle (boost::shared_ptr<Array> arr,
		 des_t desc)
	: _arr		(arr),
	  _desc		(desc)
	{}

    static des_t insert_arr (const boost::shared_ptr<Array>& arr);

    boost::shared_ptr<Array> _arr;
    
    des_t _desc;

    //
    // static
    //

    typedef std::map<int, ArrayHandle> map_t;
    /// the array map
    static map_t _arrays;

    static int _next_array_num;

};



CLOSE_NS


#endif // _CARD_ARRAY_H
