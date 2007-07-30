#include "run-circuit.h"

#include "utils.h"

#include <pir/card/configs.h>
#include <pir/common/logging.h>

#include <string>

#include <iostream>


namespace
{
    Log::logger_t logger = Log::makeLogger ("circuit-vm.card.run-circuit.test");
}


static void usage (const char* progname)
{
    using namespace std;
    
    cout << "Usage: " << progname << " [-p <card server por>]" << endl
	 << "\t[-d <crypto dir>]" << endl
	 << "\t[-c (use 4758 crypto hw?)]" << endl
	 << "\t[-n <circuit name>]" << endl
	 << "Runs named circuit (that card_server is accessing)" << endl;
}




int main (int argc, char *argv[])
{

    using namespace std;

    auto_ptr<CryptoProviderFactory> provfact;

    
    set_new_handler (out_of_memory_coredump);

    init_default_configs ();
    if ( do_configs (argc, argv) != 0 )
    {
	usage (argv[0]);
	exit (EXIT_SUCCESS);
    }

    try {
	provfact = init_crypt (g_configs);

	pir::CircuitEval evaluator (g_configs.cct_name, provfact.get());

	LOG (Log::INFO, logger,
	     "run-circuit starting circuit evaluation at "
	     << epoch_time);
	
	evaluator.eval ();
	
	LOG (Log::INFO, logger,
	     "run-circuit done with circuit evaluation at " << epoch_time);

    }
    catch (const std::exception & ex) {
	LOG (Log::ERROR, logger, "Fatal error while running circuit: "
	     << ex.what());
	exit (EXIT_FAILURE);
    }
    
}
