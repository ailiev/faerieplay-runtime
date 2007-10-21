/*
 * Circuit virtual machine for the Faerieplay hardware-assisted secure
 * computation project at Dartmouth College.
 *
 * Copyright (C) 2003-2007, Alexander Iliev <sasho@cs.dartmouth.edu> and
 * Sean W. Smith <sws@cs.dartmouth.edu>
 *
 * All rights reserved.
 *
 * This code is released under a BSD license.
 * Please see LICENSE.txt for the full license and disclaimers.
 *
*/


#include "prep-circuit.h"

#include <iostream>
#include <fstream>

#include <pir/common/consts.h>
#include <pir/common/exceptions.h>
#include <pir/card/configs.h>
#include <pir/common/logging.h>

#include <common/consts-sfdl.h>


using namespace std;

namespace
{
    Log::logger_t logger = Log::makeLogger ("circuit-vm.card.prep-circuit.test");
}



void usage (char *argv[]) {
    cerr << "Usage: " << argv[0] << " <circuit file> -n <circuit name>" << endl
	 << "\t[-d <output directory>] default=" << STOREROOT << endl
	 << endl
	 << "Produces files:" << endl
	 << CCT_CONT << ": container with the circuit gates, in text encoding,\n"
	"\tand ordered topologically." << endl
	 << GATES_CONT << ": container with the circuit gates, in text encoding,\n"
	"\tand with gate number g in cont[g]." << endl
	 << VALUES_CONT << ": container with the circuit values,\n"
	"\tinitially blank except for the inputs" << endl;
}



int main (int argc, char *argv[]) {

    int num_gates;

    auto_ptr<CryptoProviderFactory> provfact;


    opterr = 0;			// shut up error messages from getopt
    init_default_configs ();
    if ( do_configs (argc, argv) != 0 ) {
	LOG (Log::ERROR, logger, "Crypto option parsing failed");
	usage (argv);
	exit (EXIT_SUCCESS);
    }


    

    LOG (Log::DEBUG, logger, "optind = " << optind);
    LOG (Log::INFO, logger,
	 "using circuit file " << argv[optind]);

    ifstream gates_in;
    gates_in.open  (argv[optind]);
    if (!gates_in) {
	LOG (Log::CRIT, logger,
	     "Failed to open circuit file " << argv[optind] << ": " << errmsg);
	exit (EXIT_FAILURE);
    }
    

    
    
    // initialize crypto
    try
    {
	provfact = init_crypt (g_configs);
    }
    catch (const crypto_exception& ex)
    {
	LOG (Log::CRIT, logger,
	     "Error making crypto providers: " << ex.what());
	exit (EXIT_FAILURE);
    }


    // and prepare the circuit and any input array containers.
    try {
	num_gates = prepare_gates_container (gates_in, g_configs.cct_name,
					     provfact.get());
    }
    catch (const std::exception & ex) {
	LOG (Log::CRIT, logger,
	     "Error while preparing the gates, values and array containers "
	     "for execution: " << ex.what());
	exit (EXIT_FAILURE);
    }
    
}
