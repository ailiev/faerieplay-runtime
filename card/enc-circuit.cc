// program to encrypt a circuit object on the host. the circuit was produced in
// cleartext by "prep-circuit.cc", and we want to MAC it before running it.
// Since the SymWrapper class does enc and MAC together, we'll just do both.
//
// reads from the container CCT_CONT, and writes the encrypted values back in
// there.

#include <string>
#include <stdexcept>

#include <iostream>

#include <pir/card/hostcall.h>
#include <pir/card/lib.h>
#include <pir/card/consts.h>
#include <pir/card/4758_sym_crypto.h>
#include <pir/card/configs.h>

#include <pir/common/comm_types.h>
#include <pir/common/sym_crypto.h>
#ifndef _NO_OPENSSL
#include <pir/common/openssl_crypto.h>
#endif


#include <common/gate.h>
#include <common/misc.h>
#include <common/consts-sfdl.h>



using namespace std;


static void exception_exit (const std::exception& ex, const string& msg) {
    cerr << msg << ":" << endl
	 << ex.what() << endl
	 << "Exiting ..." << endl;
    exit (EXIT_FAILURE);
}


static void usage (const char* progname) {
    cerr << "Usage: " << progname << " [-p <card server por>]" << endl
	 << "\t[-d <crypto dir>]" << endl
	 << "\t[-c (use 4758 crypto hw?)]" << endl;
}


int main (int argc, char *argv[])
{

    // shut up clog for now
    //    clog.rdbuf(NULL);



    //
    // process options
    //

    init_default_configs ();
    

    int opt;
    while ((opt = getopt(argc, argv, "p:d:c")) != EOF) {
	switch (opt) {

	case 'p':
	    g_configs.host_serv_port = atoi(optarg);
	    break;
	    
	case 'd':		//directory for keys etc
	    g_configs.crypto_dir = optarg;
	    break;

	case 'c':
	    g_configs.use_card_crypt_hw = true;
	    break;

	default:
	    usage(argv[0]);
	    exit (EXIT_SUCCESS);
	}
    }

#ifdef _NO_OPENSSL
    if (!g_configs.use_card_crypt_hw) {
	cerr << "Warning: OpenSSL not compiled in, will try to use 4758 crypto"
	     << endl;
	g_configs.use_card_crypt_hw = true;
    }
#endif


    //
    // crypto setup
    //
    
    ByteBuffer mac_key (20),	// SHA1 HMAC key
	enc_key (24);		// TDES key

    // read in the keys
    // FIXME: this is a little too brittle, with all the names hardwired
    loadkeys (enc_key, g_configs.crypto_dir + DIRSEP + ENC_KEY_FILE,
	      mac_key, g_configs.crypto_dir + DIRSEP + MAC_KEY_FILE);

    auto_ptr<CryptoProviderFactory>	provfact;

    try {
	if (g_configs.use_card_crypt_hw) {
	    provfact.reset (new CryptProvFactory4758());
	}
#ifndef _NO_OPENSSL
	else {
	    provfact.reset (new OpenSSLCryptProvFactory());
	}
#endif
	
    }
    catch (const crypto_exception& ex) {
	exception_exit (ex, "Error making crypto providers");
    }

    SymWrapper symrap (enc_key, mac_key, provfact.get());


    //
    // and do the work ...
    //

    try {
	ByteBuffer obj_bytes;

	string conts[] = { CCT_CONT, VALUES_CONT };
	
	// go through all the containers
	for (unsigned c = 0; c < ARRLEN(conts); c++) {
	    
	    clog << "*** Working on container " << conts[c] << endl;
	    
	    size_t num_objs = host_get_cont_len (conts[c]);
	    
	    // and each object in the container
	    for (unsigned i=0; i < num_objs; i++) {

		host_read_blob (conts[c],
				object_name_t (object_id (i)),
				obj_bytes);
		
		// skip 0-length objects
		if (obj_bytes.len() == 0) {
		    continue;
		}
		
		obj_bytes = symrap.wrap (obj_bytes);

		clog << "Writing " << obj_bytes.len() << " bytes for object " <<
		    i << endl;
		
		host_write_blob (conts[c],
				 object_name_t (object_id (i)),
				 obj_bytes);
	    }
	}
    }
    catch (const std::exception & ex) {
	cerr << "Exception: " << ex.what() << endl;
	exit (EXIT_FAILURE);
    }
    
}
