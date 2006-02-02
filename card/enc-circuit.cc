// program to encrypt a circuit object on the host. the circuit was produced in
// cleartext by "prep-circuit.cc", and we want to MAC it before running it.
// Since the SymWrapper class does enc and MAC together, we'll just do both.
//
// reads from the container CCT_CONT, and writes the encrypted values back in
// there.

#include <string>
#include <stdexcept>

#include <iostream>

#include <boost/shared_ptr.hpp>
#include <boost/optional/optional.hpp>
#include <boost/none.hpp>


#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/lib.h>
#include <pir/card/configs.h>
#include <pir/card/io_filter.h>
#include <pir/card/io_filter_encrypt.h>

#include <pir/common/comm_types.h>
#include <pir/common/sym_crypto.h>


#include <common/gate.h>
#include <common/misc.h>
#include <common/consts-sfdl.h>



using namespace std;

using boost::shared_ptr;


namespace {
    unsigned s_log_id = Log::add_module ("enc-circuit");
}



static void exception_exit (const std::exception& ex, const string& msg) {
    cerr << msg << ":" << endl
	 << ex.what() << endl
	 << "Exiting ..." << endl;
    exit (EXIT_FAILURE);
}


static void usage (const char* progname) {
    cerr << "Usage: " << progname << " [-p <card server por>]" << endl
	 << "\t[-d <crypto dir>]" << endl
	 << "\t[-c (use 4758 crypto hw?)]" << endl
	 << "\t<circuit name>" << endl;
}


int main (int argc, char *argv[])
{

    // shut up clog for now
    //    clog.rdbuf(NULL);


    init_default_configs ();
    if ( do_configs (argc, argv) != 0 ) {
	usage (argv[0]);
	exit (EXIT_SUCCESS);
    }

    auto_ptr<CryptoProviderFactory> provfact;

    try {
	provfact = init_crypt (g_configs);
    }
    catch (const crypto_exception& ex) {
	exception_exit (ex, "Error making crypto providers");
    }


    //
    // and do the work ...
    //

    FlatIO  vals_io (g_configs.cct_name + DIRSEP + VALUES_CONT, boost::none);
    FlatIO  cct_io  (g_configs.cct_name + DIRSEP + CCT_CONT, boost::none);

    try {
	ByteBuffer obj_bytes;

	FlatIO *conts[] = { &vals_io, &cct_io };
	
	// go through all the containers
	for (unsigned c = 0; c < ARRLEN(conts); c++) {
	    
//	    clog << "*** Working on container " << io->getName() << endl;
	    
	    FlatIO * io = conts[c];
	    
	    size_t num_objs = io->getLen();
	    
	    // temp container to take encrypted objects
	    FlatIO temp (io->getName() + "-enc",
			 num_objs);

	    temp.appendFilter (auto_ptr<HostIOFilter>
			       (new IOFilterEncrypt (&temp,
						     shared_ptr<SymWrapper>
						     (new SymWrapper
						      (provfact.get())))));
	    // and transfer each object in the container
	    for (unsigned i=0; i < num_objs; i++) {

		io->read (i, obj_bytes);
		
		LOG (s_log_id, "Writing "
		     << obj_bytes.len() << " bytes for object " << i);
		
		temp.write (i, obj_bytes);
	    }

	    temp.flush ();

	    // and move encrypted container back to original name.
	    *io = temp;
	}
    }
    catch (const std::exception & ex) {
	cerr << "Exception: " << ex.what() << endl;
	exit (EXIT_FAILURE);
    }
    
}
