/* -*- c++ -*-
 * ** PIR Private Directory Service prototype
 * ** Copyright (C) 2002 Alexander Iliev <iliev@nimbus.dartmouth.edu>
 * **
 * ** This program is free software; you can redistribute it and/or modify
 * ** it under the terms of the GNU General Public License as published by
 * ** the Free Software Foundation; either version 2 of the License, or
 * ** (at your option) any later version.
 * **
 * ** This program is distributed in the hope that it will be useful,
 * ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 * ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * ** GNU General Public License for more details.
 * **
 * ** You should have received a copy of the GNU General Public License
 * ** along with this program; if not, write to the Free Software
 * ** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 * */

/* -*-c++-*-
 * batcher-permute.h
 * the database shuffling code
 */



#include <vector>

#include <stdlib.h>

#include <boost/shared_ptr.hpp>

#include <pir/common/utils.h>
#include <pir/common/exceptions.h>
#include <pir/common/sym_crypto.h>

#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/permutation.h>


#ifndef _SHUFFLE_H
#define _SHUFFLE_H


class Shuffler {

public:

    // init a Shuffler reading from recs_container, using all the provided
    // crypto details for encrypt and MAC, and using the provided permutation
    Shuffler (boost::shared_ptr<FlatIO> io,
	      boost::shared_ptr<ForwardPermutation> perm,
	      size_t			N)
        throw (crypto_exception);


    void shuffle ()
	throw (hostio_exception, crypto_exception, runtime_exception);


private:

//    SymWrapper _sym_wrapper;

    boost::shared_ptr<FlatIO> _io;

    boost::shared_ptr<ForwardPermutation> _p;
    size_t N;

    // how big to set the read-ahead cache?
    size_t _read_cache_size;

    
    typedef obj_list_t rec_list_t;
    

    //
    // class to do a comparator in the Batcher network
    //
    class Comparator {

    public:
	
	Comparator (Shuffler & master,
		    size_t max_batch_size)
	    : master		(master),
	      _max_batch_size	(max_batch_size),
	      _batch_size	(0),
	      _comparators	(_max_batch_size),
	      _idxs_temp	(_max_batch_size * 2)
	    {}

	void operator () (index_t a, index_t b)
	    throw (hostio_exception, crypto_exception);

	~Comparator ();

    private:
	Shuffler & master;

	// how many switches to store until we bulk-switch them all
	size_t _max_batch_size;

	/// how many comparators now cached?
	size_t _batch_size;
	
	struct comp_params_t {
	    index_t a, b;
	};
	
	/// the actual stored comparators, #_batch_size of them being valid.
	std::vector<comp_params_t> _comparators;


	// for storing record indices to be fetched
	std::vector<index_t> _idxs_temp;

	// list to collect record-pair addresses representing
	// comparators still to be done
	
	// do the cached comparisons.
	void do_batch ()
	    throw (hostio_exception, crypto_exception);
    
	void build_idx_list (std::vector<index_t> & o_ids,
			     const std::vector<comp_params_t>& switches);

	void do_comparators (rec_list_t & io_recs,
			     const std::vector<comp_params_t>& switches);
    };

    friend class Comparator;
    
    //
    // helpers
    //

    // remove the dest tags from a DB
    void remove_tags () throw (hostio_exception, crypto_exception);

    void prepare () throw (better_exception);
    
    // the size in bytes of the destination tag which we append to records
    static const size_t TAGSIZE = 4;
    
};


#endif // _SHUFFLE_H
