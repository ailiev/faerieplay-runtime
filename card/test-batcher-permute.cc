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


#include "batcher-permute.h"

#include <vector>
#include <fstream>
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


#include <pir/card/io_filter_encrypt.h>


using namespace std;
using namespace pir;

using boost::shared_ptr;


namespace {
    void out_of_memory () {
	cerr << "Out of memory!" << endl;
	raise (SIGTRAP);
	exit (EXIT_FAILURE);
    }
}


int main (int argc, char * argv[]) {


    // test: construct a container, encrypt it, permute it, and read
    // out the values.

    size_t N = 64;

    bool just_help = false;
    
    int opt;
    opterr = 0;			// shut up error messages from getopt
    while ((opt = getopt(argc, argv, "N:h")) != EOF) {
	switch (opt) {

	case 'N':		// directory for keys etc
	    N = atoi(optarg);
	    break;

	case 'h':		// help
	    cout << "Usage: " << argv[0] << " [-N <number of integers>]" << endl;
	    just_help = true;
	    break;
	    
	default:
	    clog << "Got opt " << char(opt)
		 << ", optarg=" << (optarg ? optarg : "null")
		 << ", optind=" << optind << endl;
	    break;
// 	    usage(argv);
// 	    exit (EXIT_SUCCESS);
	}
    }

    // reset option parsing and call do_configs
    optind = 0;

    init_default_configs ();
    g_configs.cryptprov = configs::CryptAny;
    
    if ( do_configs (argc, argv) != 0 ) {
	configs_usage (cerr, argv[0]);
	exit (EXIT_SUCCESS);
    }


    if (just_help)
    {
	// done by now
	exit (EXIT_SUCCESS);
    }
    
    auto_ptr<CryptoProviderFactory> prov_fact = init_crypt (g_configs);

    shared_ptr<SymWrapper>   io_sw (new SymWrapper (prov_fact.get()));

    // split up the creation of io_ptr and its shared_ptr, so that we can deref
    // io_ptr and pass it to IOFilterEncrypt. The shared_ptr would not be ready
    // for the operator* at that point.
    shared_ptr<FlatIO> io (new FlatIO ("permutation-test-ints",
				       make_pair (N, sizeof(int) + 4)));
//     io->appendFilter (auto_ptr<HostIOFilter> (
// 			  new IOFilterEncrypt (io.get(), io_sw)));

    // write in the consecutive integers
    for (unsigned i = 0; i < N; i++) {
	hostio_write_int (*io, i, i);
    }
    
    shared_ptr<TwoWayPermutation> pi0
	(new UnbalancedLRPermutation (lgN_ceil(N), lgN_ceil(N), prov_fact.get()));
    pi0->randomize ();

    shared_ptr<TwoWayPermutation> pi (new RangeAdapterPermutation (pi0, N));

    cout << "The permutation's mapping:" << endl;
    write_all_images (cout, pi.get());
    cout << "The permutation's inverse mapping:" << endl;
    write_all_preimages (cout, pi.get());
    
    Shuffler s (io, pi, N);
    s.shuffle ();


    cout << "And the network's result:" << endl;
    for (unsigned i = 0; i < N; i++) {
	unsigned j = hostio_read_int (*io, i);
	if (pi->d(i) != j)
	{
	    cerr << "** error at i=" << i << ": pi gives " << pi->d(i)
		 << ", network gave " << j << endl;
	}
	cout << i << " -> " << j << endl;
    }
}
