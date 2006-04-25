#include "array.h"

#include "batcher-permute.h"
#include "utils.h"

#include <stream/helpers.h>
#include <stream/processor.h>

#include <pir/common/sym_crypto.h>
#include <pir/card/lib.h>
#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/io_filter_encrypt.h>

#include <string>
#include <algorithm>		// for swap(), max and min

#include <math.h>

#include <boost/shared_ptr.hpp>
#include <boost/optional/optional.hpp> 
#include <boost/none.hpp>





#include <fstream>
#include "pir/card/configs.h"


using namespace pir;


int main (int argc, char *argv[])
{
    // create an array, fill it in with provided data from a text file, then run
    // reads/updates from another text file

    using namespace std;
    
    if (argc > 1 && std::string(argv[1]) == "--help") {
	std::cout
	    << "Reads a series of commands from file \"array-test-cmds.txt\",\n"
	    "and produces a log of their execution on stdout" << std::endl;
	exit(0);
    }
    
    
    init_default_configs ();
    g_configs.cryptprov = configs::CryptAny;
    
    do_configs (argc, argv);

    auto_ptr<CryptoProviderFactory> prov_fact = init_crypt (g_configs);

    
#include "array-test-sizes.h"
    
    Array test ("test-array",
		Just (make_pair ((size_t)ARR_ARRAYLEN, (size_t)ARR_OBJSIZE)),
		prov_fact.get());
    
    ifstream cmds ("array-test-cmds.txt");
//    cmds.exceptions (ios::badbit | ios::failbit);
    
    string cmd, idx_s, val;

    unsigned i = 0;

    while (getline (cmds, cmd) &&
	   getline (cmds, idx_s))
    {
	index_t idx = atoi (idx_s.c_str());
	
	cout << endl << "Command " << i++ << ": " << cmd << " [" << idx << "] ";

	if (cmd == "write") {
	    getline (cmds, val);
	    cout << "(" << val << ")";
	    test.write (idx, 0, ByteBuffer (val));
	    cout << " --> ()";
	}
	else if (cmd == "read") {
	    cout << "()";
	    ByteBuffer res = test.read (idx);

	    cout << " --> (";
	    cout.write (res.cdata(), res.len());
	    cout << ")" << endl;
	}
    }

    cout << "Array test run finished @ " << epoch_time << endl;

}

