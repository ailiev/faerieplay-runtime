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
    shared_ptr<FlatIO> io (new FlatIO ("permutation-test-ints",
				       make_pair (N, sizeof(int))));
    io->appendFilter (auto_ptr<HostIOFilter> (
			  new IOFilterEncrypt (io.get(), io_sw)));

    for (unsigned i = 0; i < N; i++) {
	hostio_write_int (*io, i, i);
    }
    
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
