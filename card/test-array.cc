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


//
// the array testing setup:
//
// \file{test-array.cc} reads commands from \file{array-test-cmds.txt} and
// carries them out using an Array object.
// 
// several parameters are specified in \file{array-test-sizes.h}. They are used
// in \file{test-array.cc}, as well as by \file{array-test-gen.sh}, which
// generates a random sequence of read/write commands, into
// \file{array-test-cmds.txt}.
//
// \file{check-array-test-run.pl} takes the output of test-array on stdin, and
// runs the commands itself and reports any discrepancies from test-array's
// output.


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

    //
    // first do a test of printing the array out
    //
#ifdef LOGVALS
    const size_t PRINTTEST_LEN = 32;
    Array printtest ("printtest-array",
		     Just (make_pair (PRINTTEST_LEN, sizeof(int))),
		     prov_fact.get());

    for (unsigned i=0; i < PRINTTEST_LEN; i++) {
	ByteBuffer b = basic2bb (1 << i);
	printtest.write (true, i, 0, b);
    }

    // redo some of the writes to make sure that T has some elements in it.
    for (unsigned i=0; i < PRINTTEST_LEN; i += 4) {
	ByteBuffer b = basic2bb (1 << i);
	printtest.write (true, i, 0, b);
    }
	
    // and put in some of those infamous all-1 bitpatterns
    ByteBuffer minusone = basic2bb (-1);
    for (unsigned i=0; i < PRINTTEST_LEN; i += 4) {
	printtest.write (true, i, 0, minusone);
    }

    Array::print (cout, printtest);
#endif // LOGVALS


    //
    // and now the write-read consistency test
    //
    
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
	    test.write (true, idx, 0, ByteBuffer (val));
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

