// Driver file for the Faerieplay circuit virtual machine

#include "prep-circuit.h"
#include "enc-circuit.h"
#include "run-circuit.h"

#include "utils.h"

#include <pir/common/sym_crypto.h>
#include <pir/card/configs.h>

#include <fstream>

#include <memory>


using namespace std;


auto_ptr<CryptoProviderFactory> g_provfact;



namespace
{
    Log::logger_t logger = Log::makeLogger ("circuit-vm.card.cvm");
}


void usage (char *argv[])
{
    cerr << "Usage: " << argv[0] << " <circuit file> < input" << endl;
}




int main (int argc, char * argv[])
{

    set_new_handler (out_of_memory_coredump);

    string cct_filename;

    opterr = 0;			// shut up error messages from getopt
    init_default_configs ();
    if ( do_configs (argc, argv) != 0 ) {
	LOG (Log::ERROR, logger, "Command line parsing failed");
	usage (argv);
	exit (EXIT_SUCCESS);
    }

    if (g_configs.just_help) {
	usage (argv);
	exit (EXIT_SUCCESS);
    }

    if (optind >= argc) {
	usage (argv);
	exit (EXIT_SUCCESS);
    }
    
    cct_filename = argv[optind];
    LOG (Log::INFO, logger, "using circuit file " << cct_filename);


    //
    // initialize crypto
    //
    try
    {
	g_provfact = init_crypt (g_configs);
    }
    catch (const crypto_exception& ex)
    {
	LOG (Log::CRIT, logger,
	     "Error making crypto providers: " << ex.what());
	exit (EXIT_FAILURE);
    }


    //
    // open circuit file
    //
    ifstream gates_in;
    gates_in.open  (cct_filename.c_str());
    if (!gates_in) {
	LOG (Log::CRIT, logger,
	     "Failed to open circuit file " << cct_filename << ": " << errmsg);
	exit (EXIT_FAILURE);
    }
    

    //
    // prepare the circuit and any input array containers.
    //
    try {
	size_t num_gates = prepare_gates_container (gates_in, g_configs.cct_name,
						    g_provfact.get());

	LOG (Log::INFO, logger,
	     "Circuit has " << num_gates << " gates");
    }
    catch (const std::exception & ex) {
	LOG (Log::CRIT, logger,
	     "Error while preparing the gates, values and array containers "
	     "for execution: " << ex.what());
	exit (EXIT_FAILURE);
    }


    //
    // encrypt the circuit containers
    //
    try {
	do_encrypt (g_provfact.get());
    }
    catch (const std::exception & ex) {
	LOG (Log::CRIT, logger,
	     "Exception while encrypting containers: " << ex.what());
	exit (EXIT_FAILURE);
    }


    //
    // and run the circuit
    //
    try {
	pir::CircuitEval evaluator (g_configs.cct_name, g_provfact.get());

	LOG (Log::INFO, logger,
	     "cvm starting circuit evaluation at " << epoch_time);
    
	evaluator.eval ();
    
	LOG (Log::INFO, logger,
	     "cvm done with circuit evaluation at " << epoch_time);
    
    }
    catch (const std::exception & ex) {
	LOG (Log::ERROR, logger,
	     "Fatal error while running circuit: " << ex.what());
	exit (EXIT_FAILURE);
    }

}
