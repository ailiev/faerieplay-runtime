/*
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

// shuffle.cc
// alex iliev jan 2003
// routines for PIR database shuffling, in the Shuffler class


#include <vector>
#include <fstream>
#include <algorithm>
#include <functional>
#include <exception>

#include <boost/shared_ptr.hpp>
#include <boost/range.hpp>

#include <signal.h>		// for SIGTRAP


#include <pir/common/logging.h>
#include <pir/common/exceptions.h>
#include <pir/common/sym_crypto.h>

#include <pir/card/lib.h>
#include <pir/card/configs.h>

#include "stream/processor.h"
#include "stream/helpers.h"

#include "utils.h"
#include "batcher-network.h"

#include "batcher-permute.h"




static char *rcsid ="$Id$";

using boost::shared_ptr;

using std::vector;



namespace {
    Log::logger_t logger = Log::makeLogger ("circuit-vm.card.batcher-permute");
}




OPEN_NS


Shuffler::Shuffler (shared_ptr<FlatIO> container,
		    shared_ptr<ForwardPermutation> p,
		    size_t N)
    throw (crypto_exception)
    : _io   (container),
      _p    (p),
      N	    (N),
      _read_cache_size (CACHEMEM / _io->getElemSize())
{}





void Shuffler::shuffle ()
    throw (hostio_exception, crypto_exception, runtime_exception)
{
    // add the destination index tags to all the records
    prepare ();

    LOG (Log::PROGRESS, logger,
	 "Start DB shuffle @ " << epoch_secs());
    

    // perform the permutation using our own switch function object to perform
    // the actual switches
    // TODO: figure out how to set the batch size better
    {
	// a batch element for the comparator is actually two objects
	// make sure the batch size divides N.
	unsigned batchsize = std::min (_read_cache_size/2, N-1);

	Comparator comparator (*this, batchsize);
	run_batcher (N, comparator, &comparator);
    }

    LOG (Log::PROGRESS, logger,
	 "Shuffle done @ " << epoch_secs());

    // now need to remove the tags of the records
    // could have been good to do it at the last stage of sorting
    // though, need to think about this
    remove_tags ();
}


// tag an item with its destination index, at the end of the actual data
struct Shuffler::tagger {

    tagger (ForwardPermutation & pi)
	: pi (pi)
	{}
	
    void operator() (index_t i, const ByteBuffer& in, ByteBuffer& out) const
	{
	    index_t dest = pi.p(i);

	    out = realloc_buf (in, in.len() + TAGSIZE);
	    memcpy ( out.data() + in.len(), &dest, TAGSIZE );
	}

    ForwardPermutation & pi;
};



void Shuffler::prepare () throw (better_exception)
{
    stream_process ( tagger(*_p),
		     zero_to_n (N),
		     _io.get(),
		     _io.get());
}


struct Shuffler::tag_remover {
    
    void operator() (index_t, const ByteBuffer& in, ByteBuffer& out) const
	{
	    out = in;
	    out.len() -= TAGSIZE;
	}
};
    

void Shuffler::remove_tags () throw (hostio_exception, crypto_exception)
{
    stream_process (tag_remover(),
		    zero_to_n (N),
		    _io.get(),
		    _io.get());
}



void Shuffler::Comparator::operator () (index_t a, index_t b)
    throw (better_exception)
{

    assert (a < master.N && b < master.N);
    
    // Now we will collect bucket-pairs to switch, and switch them in
    // bulk when we have enough

    // add this switch to the list
    comp_params_t this_one = { a, b };
    _comparators[_batch_size] = this_one;
    ++ _batch_size;


    // is it time to do them?
    if (_batch_size == _max_batch_size) {
	do_batch();
	// reset batch
	_batch_size = 0;
    }
}

void Shuffler::Comparator::newpass ()
    throw (better_exception)
{
    LOG (Log::DEBUG, logger,
	 "doing " << _batch_size << " batched comparators on new pass");
	
    // flush the batched comparators
    if (_batch_size > 0)
    {
	do_batch();
	_batch_size = 0;
    }
}


OPEN_ANON_NS
// notice it's a fully copied parameter
void check_for_dups (vector<index_t> V)
{
    std::sort (V.begin(), V.end());

    vector<index_t>::iterator dup;

    for ( dup = std::adjacent_find (V.begin(), V.end());
	  dup != V.end();
	  dup = std::adjacent_find (++dup, V.end()))
    {
	LOG (Log::ERROR, logger,
	     "do_batch has index " << *dup << " more than once!");
    }
}
CLOSE_NS


void Shuffler::Comparator::do_batch ()
    throw (hostio_exception, crypto_exception)
{
    obj_list_t objs (_batch_size * 2);


    // XXX: this needed in case do_batch is called with less-than-full
    // _comparators, ie. before _max_batch_size comparators were batched.
    // Hopefully this does not cause re-alocation of the vectors.
    _comparators.resize(_batch_size);
    _idxs_temp.resize (_batch_size * 2);
    
    // first build a list of index_t for all the records involved
    build_idx_list (_idxs_temp, _comparators);

    // there should be no duplicated in the index list, or the comparators will
    // not be done correctly.
//     LOG (Log::DEBUG, logger,
// 	 "do_batch() checking for dups on batch of size " << _batch_size);
//     check_for_dups (_idxs_temp);

    // now fetch and decrypt all the blobs.
    master._io->read (_idxs_temp, objs);
	
    // now do the switches inside the blobs list
    do_comparators (objs, _comparators);

    // and now write out the switched blobs
    master._io->write (_idxs_temp, objs);

    // XXX: restore the caches to the expected size.
    _comparators.resize (_max_batch_size);
    _idxs_temp.resize (_max_batch_size*2);
}


Shuffler::Comparator::~Comparator ()
{
    LOG (Log::DEBUG, logger,
	 "~Comparator(), with _batch_size = " << _batch_size);
    
    if (_batch_size > 0) {
	LOG (Log::WARN, logger,
	     "Comparator doing " << _batch_size
	     << " remaing comparators at the end");
	do_batch ();
    }
}

void Shuffler::Comparator::build_idx_list (
    std::vector<index_t> & o_ids,
    const std::vector<comp_params_t>& comps)
{
    // o_ids should be preallocated
    assert (o_ids.size() == 2 * comps.size());

    std::vector<comp_params_t>::const_iterator s;
    std::vector<index_t>::iterator idx = o_ids.begin();
    for (s = comps.begin(); s != comps.end(); s++) {
	*idx = s->a; ++idx;
	*idx = s->b; ++idx;
    }
}


// internally do a series of switches on a flat list of records,
// ordered bucket by bucket, ie. if the buckets are a1,b1,a2,b2,...
// and each has 5 records,
// the object_ids in the list will be:
// (a1,0),(a1,1),...,(a1,5),(b1,0),...,(b1,5),(a2,0),...

void
Shuffler::Comparator::do_comparators (Shuffler::rec_list_t & io_recs,
				      const std::vector<comp_params_t>& comps)
{
    // pointer to the first blob of the two buckets of the current
    // switch, and a temporary iterator
    Shuffler::rec_list_t::iterator r1, r2;

    LOG (Log::DUMP, logger,
	 "Batch list size at do_comparators() is " << io_recs.size());
    
    r1 = io_recs.begin(); // record 1, right at the beginning
    // record 2, next one in list
    r2 = iterator_add (r1, 1);

    FOREACH (c, comps) {
	
	// get the destination tags of the records
	uint32_t a, b;
	memcpy (&a, r1->data() + r1->len() - TAGSIZE, TAGSIZE);
	memcpy (&b, r2->data() + r2->len() - TAGSIZE, TAGSIZE);
	
	assert (a < master.N && b < master.N);
	
	if (b < a) {
	    // do the switch
	    
	    // copying ByteBuffer's is cheap, so we may as well use swap()
	    // instead of list splicing which is more complex
	    // in fact if vectors are used splicing isnt much of an option!
	    std::swap (*r1, *r2);

	}
	else {
	    // FIXME: should probably do something else that takes
	    // same amount of time as the switching operation
	}

	std::advance (r1, 2);
	std::advance (r2, 2);
    }

}


CLOSE_NS
