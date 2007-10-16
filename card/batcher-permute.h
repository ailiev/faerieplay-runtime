// -*- c++ -*-
/*
Copyright (C) 2003-2007, Alexander Iliev <sasho@cs.dartmouth.edu>

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.
* Neither the name of the author nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.

This product includes cryptographic software written by Eric Young
(eay@cryptsoft.com)

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


/*
 * batcher-permute.h
 * the database shuffling code
 */


#include "batcher-network.h"

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
