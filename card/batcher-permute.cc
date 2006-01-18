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

#include <unistd.h>		// for getopt()
#include <time.h>		// for time()
#include <signal.h>		// for SIGTRAP


#include <common/exceptions.h>
#include <common/record.h>
#include <common/sym_crypto.h>
#include <common/openssl_crypto.h>
#include <common/socket-class.h>
#include <common/scc-socket.h>
#include <common/consts.h>

#include "lib.h"
#include "hostcall.h"
#include "consts.h"
#include "batcher-permute.h"
#include "4758_sym_crypto.h"
#include "io.h"


#include "batcher-shuffle.h"


static char *id ="$Id$";

using namespace std;

static const std::string CLEARDIR = "clear",
    CRYPTDIR_BASE = "crypt";


static void inform_retriever (const std::string & dbdir,
			      unsigned short retriever_cardnum)
    throw (comm_exception);



void out_of_memory () {
    cerr << "Out of memory!" << endl;
    raise (SIGTRAP);
    exit (EXIT_FAILURE);
}


static void
usage(const char* progname)
{
    fprintf(stderr,
	    "Usage: %s\n"
	    "[-p <card server port>]\n"
	    "[-d <root dir for keys etc.>]\n"
	    "[-c (means use 4758 crypto hardware instead of OpenSSL)]\n"
	    "[-r <retriever card number>]\n",
	    progname);
    exit(EXIT_FAILURE);
}



int main (int argc, char * argv[]) {


    unsigned short host_serv_port = PIR_HOSTSERV_PORT;
    unsigned short retriever_cardnum = CARDNO_RETRIEVER;
    string dir = CARD_CRYPTO_DIR;
    bool use_card_crypt_hw = true; // do we use the 4758 hardware for crypto?
    
    
    ByteBuffer keybuf, mackeybuf;

    set_new_handler (out_of_memory);

    int opt;
    while ((opt = getopt(argc, argv, "p:d:r:c")) != EOF) {
	switch (opt) {

        case 'p':		// port
	    host_serv_port = strtol(optarg, NULL, 0);
	    if (host_serv_port == 0) {
		cerr << argv[0] << ": Invalid port" << endl;
		exit (EXIT_FAILURE);
	    }
	    break;

	case 'd':		//directory for keys etc
	    dir = optarg;
	    break;

	case 'c':
	    use_card_crypt_hw = true;
	    break;

	case 'r':
	    retriever_cardnum = atoi (optarg);
	    break;
	    
	default:
	    usage(argv[0]); 
	}
    }

#ifdef _NO_OPENSSL
    if (!use_card_crypt_hw) {
	cerr << "Warning: OpenSSL not compiled in, will try to use 4758 crypto"
	     << endl;
	use_card_crypt_hw = true;
    }
#endif

    

    try {
	loadkeys (keybuf,    dir + DIRSEP + KEYFILE,
		  mackeybuf, dir + DIRSEP + MACKEYFILE);
    }
    catch (const io_exception& ex) {
	cerr << "Error loading keys:" << endl
	     << "\t" << ex.what() << endl;
	exit (EXIT_FAILURE);
    }


    auto_ptr<CryptoProviderFactory> prov_factory;

    try {
	if (use_card_crypt_hw) {
	    prov_factory.reset (new CryptProvFactory4758());
	}
#ifndef _NO_OPENSSL
	else {
	    prov_factory.reset (new OpenSSLCryptProvFactory());
	}
#endif
    }
    catch (const crypto_exception& ex) {
	cerr << "Exception from crypto provider creation!" << endl
	     << ex.what() << endl;
	return EXIT_FAILURE;
    }
    
    // DBSIZE and BUCKETSIZE are for now in lib.h

//    hash_t root_hash;

    for (int dbnum = 1; ; dbnum++) {

	std::ostringstream cryptdir_str;
	cryptdir_str << CRYPTDIR_BASE << dbnum;
	std::string cryptdir = cryptdir_str.str();
    
	try {
	    size_t blocksize = lgN_floor (DBSIZE);
	    size_t laddersize = 7;

	    auto_ptr<LRPermutation> perm
		(new LRPermutation (blocksize, laddersize, prov_factory.get()));
	    perm->randomize();
	    
	    // write it out using a one-time HostIO object
	    // commnent out while TOY_HOSTCALL_ON_CARD
	    HostIO (cryptdir).write (SHUFFLEFILE, perm->serialize());

	    // a type cast of sorts, from LRPermutation to the parent
	    // ForwardPermutation
	    auto_ptr<ForwardPermutation> fwd_perm (perm);
	    
	    Shuffler shuffler (CLEARDIR,
			       get_crypt_cont_name (cryptdir),
			       prov_factory.get(),
			       keybuf, mackeybuf,
			       fwd_perm,
			       DBSIZE);
	    
	    clog << "Making permuted database..." << endl;
	    
	    shuffler.shuffle ();

#if 0
	    // and now we need to generate the hash tree for this
	    // shuffled DB
	    clog << "Making hash tree @ " << epoch_time << endl;
	    HashTreeBuilder h (cryptdir, prov_factory.get(), DBSIZE);
 	    root_hash = h.build_hash_tree ();

	    clog << "hash tree done @ " << epoch_time << endl;

	    // FIXME: write the root hash to the crypt dir.
	    HostIO (cryptdir).write (ROOT_HASH_FILE, root_hash);
#endif


	    // inform the retriever that a new DB is ready
	    // need to also send:
	    // - shuffle vector or other key
	    // - root hash of the hash tree
//	    inform_retriever (cryptdir, retriever_cardnum);
	}
	catch (const exception& ex) {
	    cerr << "Exception from shuffle: " << ex.what() << endl
		 << "Exiting" << endl;
	    return EXIT_FAILURE;
	}

    }

}



//const size_t Shuffler::TAGSIZE = 4;


Shuffler::Shuffler (const std::string& in_container,
		    const std::string& out_container,
                    CryptoProviderFactory * provfact,
		    const ByteBuffer & key, const ByteBuffer & mackey,
		    auto_ptr<ForwardPermutation> perm,
		    size_t N)
    throw (crypto_exception)
    : _key		(key),
      _mackey		(mackey),
      _sym_wrapper	(key, mackey, provfact),
      _crypt_io		(out_container),
      _clear_io		(in_container),
      _crypt_rec_io	(_crypt_io),
      _permutation	(perm),
      N			(N)
{
	// make the host-side container for the permuted DB
	host_create_container (out_container, N, DEFAULT_MAX_OBJ_SIZE);
}





void Shuffler::shuffle ()
    throw (hostio_exception, crypto_exception)
{
    clog << "Start encrypting DB @ " << epoch_time << endl;
    
    // should first encrypt all the records to produce the encrypted
    // database, and add the destination index tags to all the records
    make_encrypted_db();

    clog << "Done with encrypted DB @ " << epoch_time << endl;
    

    // perform the permutation using our own switch function object to perform
    // the actual switches
    // TODO: figure out how to set the batch size better
    const size_t BATCHSIZE = 4;
    Comparator comparator (*this, BATCHSIZE);
    run_batcher (N, comparator);

    // now need to remove the tags of the records
    // could have been good to do it at the last stage of sorting
    // though, need to think about this
    remove_tags ();

    clog << "Shuffle done @ " << epoch_time << endl;
}



void Shuffler::make_encrypted_db () throw (hostio_exception, crypto_exception)
{
    RecBlob current;
    RecIO clear_rec_io (_clear_io);
    uint32_t dest;
    
    // read, encrypt and write all the records, no big deal
    // added: also tag them with their destination index

    for (index_t i=0; i < N; i++) {
	// read
	clear_rec_io.read_record (i, current);

	// add tag, using 4 bytes at the end
	dest = _permutation->p (i);
	current = realloc_buf (current, current.len() + TAGSIZE);
	memcpy ( current.data() + current.len() - TAGSIZE, &dest, TAGSIZE );
	
	// encrypt
	current = _sym_wrapper.wrap (current);

	// write
	_crypt_rec_io.write_record (i, current);
    }
}


void Shuffler::remove_tags () throw (hostio_exception, crypto_exception)
{
    RecBlob current;
    

    for (index_t i=0; i < N; i++) {
	// read
	_crypt_rec_io.read_record (i, current);

	// decrypt
	current = _sym_wrapper.unwrap (current);

	// just adjust the length
	current.len() -= TAGSIZE;
	
	// encrypt
	current = _sym_wrapper.wrap (current);

	// write
	_crypt_rec_io.write_record (i, current);
    }
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

	rec_list_t recs;

	// first build a list of index_t for all the records involved
	build_idx_list (_idxs_temp, _comparators);

	// now fetch and decrypt all the blobs.
	master._crypt_rec_io.read_records (_idxs_temp, recs);
	master.decrypt_recs (recs);
	
	// now do the switches inside the blobs list
	do_comparators (recs, _comparators);

	// re-encrypt the blobs
	master.encrypt_recs (recs);

	// and now write out the switched blobs
	master._crypt_rec_io.write_records (_idxs_temp, recs);

	// reset batch
	_batch_size = 0;
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

//    clog << "Batch list size at start is " << io_recs.size() << endl;
    
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

//    clog << "Batch list size at end is " << io_recs.size() << endl;

    
}



void Shuffler::decrypt_recs (rec_list_t & io_blobs)
    throw (crypto_exception)
{
    transform (io_blobs.begin(), io_blobs.end(),
	       io_blobs.begin(),
	       memfun_adapt (&SymWrapper::unwrap,
			     _sym_wrapper));
}

void Shuffler::encrypt_recs (rec_list_t & io_blobs)
    throw (crypto_exception)
{
    transform (io_blobs.begin(), io_blobs.end(),
	       io_blobs.begin(),
	       memfun_adapt (&SymWrapper::wrap,
			     _sym_wrapper));
}



void inform_retriever (const std::string & dbdir,
		       unsigned short retriever_cardnum)
    throw (comm_exception)
{

    SCCDatagramSocket sock;
    SCCSocketAddress retriever_addr (retriever_cardnum, PIR_CONTROL_PORT);

    sock.sendto (ByteBuffer (dbdir), retriever_addr);
}


// transpose is used a bit loosely here
// we just want to set V_[V[i]] = i for all indices i
void transpose_vector (vector<index_t> & V_, const vector<index_t>& V) {

    V_.resize (V.size());

    for (unsigned i = 0; i < V.size(); i++) {
	V_[V[i]] = i;
    }
}
