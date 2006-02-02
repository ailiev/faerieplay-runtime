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
#include <functional>
#include <exception>

#include <boost/shared_ptr.hpp>

#include <signal.h>		// for SIGTRAP


#include <pir/common/logging.h>
#include <pir/common/exceptions.h>
#include <pir/common/sym_crypto.h>

#include <pir/card/lib.h>
#include <pir/card/configs.h>

#include "utils.h"
#include "batcher-network.h"

#include "batcher-permute.h"




static char *rcsid ="$Id$";

using namespace std;

using boost::shared_ptr;

static const std::string CLEARDIR = "clear",
    CRYPTDIR_BASE = "crypt";



#ifdef _TESTING_BATCHER_PERMUTE

#include <pir/card/io_filter_encrypt.h>


void out_of_memory () {
    cerr << "Out of memory!" << endl;
    raise (SIGTRAP);
    exit (EXIT_FAILURE);
}



int main (int argc, char * argv[]) {


    // test: construct a container, encrypt it, permute it, and read
    // out the values

    init_default_configs ();
    g_configs.cryptprov = configs::CryptAny;
    
    if ( do_configs (argc, argv) != 0 ) {
	configs_usage (cerr, argv[0]);
	exit (EXIT_SUCCESS);
    }

    auto_ptr<CryptoProviderFactory> prov_fact = init_crypt (g_configs);

    size_t N = 64;

    shared_ptr<SymWrapper>   io_sw (new SymWrapper (prov_fact.get()));

    // split up the creation of io_ptr and its shared_ptr, so that we can deref
    // io_ptr and pass it to IOFilterEncrypt. The shared_ptr would not be ready
    // for the operator* at that point.
    shared_ptr<FlatIO> io (new FlatIO ("permutation-test-ints", N));
    io->appendFilter (auto_ptr<HostIOFilter> (
			  new IOFilterEncrypt (io.get(), io_sw)));

    for (unsigned i = 0; i < N; i++) {
	hostio_write_int (*io, i, i);
    }
    io->flush ();
    
    shared_ptr<TwoWayPermutation> pi (
	new LRPermutation (lgN_ceil (N), 7, prov_fact.get()) );
    pi->randomize ();

    cout << "The permutation's mapping:" << endl;
    write_all_preimages (cout, pi.get());
    
    Shuffler s (io, pi, N);
    s.shuffle ();


    cout << "And the network's result:" << endl;
    for (unsigned i = 0; i < N; i++) {
	unsigned j = hostio_read_int (*io, i);
	cout << i << " -> " << j << endl;
    }
}



#endif // _TESTING_BATCHER_PERMUTE

//const size_t Shuffler::TAGSIZE = 4;



namespace {
    unsigned s_log_id = Log::add_module ("batcher-permute");
}


Shuffler::Shuffler (shared_ptr<FlatIO> container,
		    shared_ptr<ForwardPermutation> p,
		    size_t N)
    throw (crypto_exception)
    : _io   (container),
      _p    (p),
      N	    (N)
{}





void Shuffler::shuffle ()
    throw (hostio_exception, crypto_exception, runtime_exception)
{
    LOG (Log::PROGRESS, s_log_id,
	 "Start preparing DB @ " << epoch_time);
    
    // should first encrypt all the records to produce the encrypted
    // database, and add the destination index tags to all the records
    prepare ();

    LOG (Log::PROGRESS, s_log_id,
	 "Done with preparing DB @ " << epoch_time);
    

    // perform the permutation using our own switch function object to perform
    // the actual switches
    // TODO: figure out how to set the batch size better
    {
	const size_t BATCHSIZE = 4;
	Comparator comparator (*this, BATCHSIZE);
	run_batcher (N, comparator);
    }

    // now need to remove the tags of the records
    // could have been good to do it at the last stage of sorting
    // though, need to think about this
    remove_tags ();

    LOG (Log::PROGRESS, s_log_id,
	 "Shuffle done @ " << epoch_time);
}



void Shuffler::prepare () throw (better_exception)
{
    ByteBuffer current;
    uint32_t dest;
    
    // tag all items with their destination index

    for (index_t i=0; i < N; i++) {
	// read
	_io->read (i, current);

	// add tag, using 4 bytes at the end
	dest = _p->p (i);
	current = realloc_buf (current, current.len() + TAGSIZE);
	memcpy ( current.data() + current.len() - TAGSIZE, &dest, TAGSIZE );
	
	// write
	_io->write (i, current);
    }

    _io->flush ();
}


void Shuffler::remove_tags () throw (hostio_exception, crypto_exception)
{
    ByteBuffer current;
    

    for (index_t i=0; i < N; i++) {
	// read
	_io->read (i, current);

	// just adjust the length
	current.len() -= TAGSIZE;
	
	// write
	_io->write (i, current);
    }

    _io->flush();
}



void Shuffler::Comparator::operator () (index_t a, index_t b)
    throw (hostio_exception, crypto_exception)
{

    // Now we will collect bucket-pairs to switch, and switch them in
    // bulk when we have enough

    // add this switch to the list
    comp_params_t this_one = { a, b };
    _comparators[_batch_size] = this_one;
    ++ _batch_size;


    // is it time to do them?
    if (_batch_size == _max_batch_size) {

	obj_list_t objs;

	// first build a list of index_t for all the records involved
	build_idx_list (_idxs_temp, _comparators);

	// now fetch and decrypt all the blobs.
	master._io->read (_idxs_temp, objs);
	
	// now do the switches inside the blobs list
	do_comparators (objs, _comparators);

	// and now write out the switched blobs
	master._io->write (_idxs_temp, objs);

	// reset batch
	_batch_size = 0;
    }
}


Shuffler::Comparator::~Comparator ()
{
    if (_batch_size > 0) {
	throw invalid_state_exception (
	    "Comparator has not done all its batched comparators!");
    }
}

void Shuffler::Comparator::build_idx_list (
    std::vector<index_t> & o_ids,
    const std::vector<comp_params_t>& comps)
{
    // PRE: o_oids has the right number of elts (2 * comps.size())
    std::vector<comp_params_t>::const_iterator s;
    std::vector<index_t>::iterator idx = o_ids.begin();
    for (s = comps.begin(); s != comps.end(); s++) {
	*idx++ = s->a;
	*idx++ = s->b;
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

    LOG (Log::DUMP, s_log_id,
	 "Batch list size at start is " << io_recs.size());
    
    r1 = io_recs.begin(); // record 1, right at the beginning
    // record 2, next one in list
    r2 = iterator_add (r1, 1);

    FOREACH (c, comps) {
	
	// get the destination tags of the records
	uint32_t a, b;
	memcpy (&a, r1->data() + r1->len() - TAGSIZE, TAGSIZE);
	memcpy (&b, r2->data() + r2->len() - TAGSIZE, TAGSIZE);
	
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

    LOG (Log::DUMP, s_log_id,
	 "Batch list size at end is " << io_recs.size());

    
}


// transpose is used a bit loosely here
// we just want to set V_[V[i]] = i for all indices i
void transpose_vector (vector<index_t> & V_, const vector<index_t>& V) {

    V_.resize (V.size());

    for (unsigned i = 0; i < V.size(); i++) {
	V_[V[i]] = i;
    }
}
