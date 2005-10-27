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

#include <pir/common/comm_types.h>
#include <pir/common/sym_crypto.h>

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



int main (int argc, char *argv[])
{
    ByteBuffer gate_bytes;
    string gate_str;
    gate_t gate;

    // shut up clog for now
//    ofstream out("/dev/null");
//    if (out)
    clog.rdbuf(NULL);

    size_t num_gates = host_get_cont_len (CCT_CONT);


    //
    // crypto setup
    //
    ByteBuffer mac_key (20),	// SHA1 HMAC key
	enc_key (24);		// TDES key

    // read in the keys
    // FIXME: this is a little too brittle, with all the names hardwired
    loadkeys (enc_key, CARD_CRYPTO_DIR + DIRSEP + ENC_KEY_FILE,
	      mac_key, CARD_CRYPTO_DIR + DIRSEP + MAC_KEY_FILE);

    auto_ptr<SymCryptProvider>	symop;
    auto_ptr<MacProvider>		macop;
    auto_ptr<CryptoProviderFactory>	provfact;

    try {
	symop.reset (new SymCrypt4758 ());
	macop.reset (new SHA1HMAC_4758());
	provfact.reset (new CryptProvFactory4758());
    }
    catch (const crypto_exception& ex) {
	exception_exit (ex, "Error making crypto providers");
    }

    SymWrapper symrap (enc_key, *symop, mac_key, *macop);



    try {
	for (unsigned i=0; i < num_gates; i++) {

	    host_read_blob (CCT_CONT,
			    object_name_t (object_id (i)),
			    gate_bytes);

	    gate_bytes = symrap.wrap (gate_bytes);
	    
	    host_write_blob (CCT_CONT,
			     object_name_t (object_id (i)),
			     gate_bytes);

	}
    }
    catch (const std::exception & ex) {
	cerr << "Exception: " << ex.what() << endl;
	exit (EXIT_FAILURE);
    }
    
}


void do_gate (int gate_num)
    
{
    ByteBuffer gate_bytes;
    
    host_read_blob (CCT_CONT,
		    object_name_t (object_id (gate_num)),
		    gate_bytes);

// 	clog << "gate_byte len: " << gate_bytes.len() << endl;
// 	    "bytes: " << gate_bytes.cdata() <<  endl;

	    
}
