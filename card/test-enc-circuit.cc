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


#include "enc-circuit.h"

#include <string>
#include <stdexcept>

#include <iostream>

#include <pir/card/configs.h>


using namespace std;


static void exception_exit (const std::exception& ex, const string& msg)
{
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


    try {
	do_encrypt (provfact.get());
    }
    catch (const std::exception & ex) {
	exception_exit (ex, "Error while encrypting containers");
    }


}
