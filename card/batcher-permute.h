// -*- c++ -*-
/*
 * Circuit virtual machine for the Faerieplay hardware-assisted secure
 * computation project at Dartmouth College.
 *
 * Copyright (C) 2003-2007, Alexander Iliev <sasho@cs.dartmouth.edu> and
 * Sean W. Smith <sws@cs.dartmouth.edu>
 *
 * All rights reserved.
 *
 * This code is released under a BSD license.
 * Please see LICENSE.txt for the full license and disclaimers.
 *
 */


/*
 * batcher-permute.h
 * the database shuffling code
 */


#include "batcher-network.h"

#include <vector>

#include <stdlib.h>

#include <boost/shared_ptr.hpp>

#include <faerieplay/common/utils.h>
#include <faerieplay/common/exceptions.h>
#include <pir/common/sym_crypto.h>

#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/permutation.h>


#ifndef _SHUFFLE_H
#define _SHUFFLE_H


OPEN_NS


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
    class Comparator : public BatcherInfoListener
    {
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
	    throw (better_exception);

	virtual void newpass () throw (better_exception);

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


    struct tag_remover;
    friend struct tag_remover;	// to read TAGSIZE

    struct tagger;
    friend struct tagger;	// for TAGSIZE
    
    //
    // helpers
    //

    // remove the dest tags from a DB
    void remove_tags () throw (hostio_exception, crypto_exception);

    void prepare () throw (better_exception);
    
    // the size in bytes of the destination tag which we append to records
    static const size_t TAGSIZE = 4;
    
};


CLOSE_NS


#endif // _SHUFFLE_H
