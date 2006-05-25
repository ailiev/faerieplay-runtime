#include <string>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utility>

//#include <pir/host/objio.h>
#include <pir/common/utils.h>
#include <pir/common/consts.h>

#include <pir/card/configs.h>
#include <pir/card/io_flat.h>

#include <common/consts-sfdl.h>
#include <common/gate.h>

#include <common/misc.h>

#include "array.h"


using namespace std;

using pir::Array;
using pir::ArrayHandle;


auto_ptr<CryptoProviderFactory> g_provfact;


int prepare_gates_container (istream & gates_in,
			     const string& cct_name)
    throw (io_exception, std::exception);


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

    string store_root = STOREROOT;
    
    ifstream gates_in;
    
    // shut up clog for now
    /*
    ofstream out("/dev/null");
    if (out)
	clog.rdbuf(out.rdbuf());
    */
    
    //
    // here we just scan for the store root directory
    //
    int opt;
    opterr = 0;			// shut up error messages from getopt
    while ((opt = getopt(argc, argv, "h")) != EOF) {
	switch (opt) {

	case 'h':		// help
	    usage (argv);
	    configs_usage (cerr, argv[0]);
	    exit (EXIT_SUCCESS);

	default:
	    clog << "Got unknown opt " << char(optopt)
		 << ", optarg=" << (optarg ? optarg : "null")
		 << ", optind=" << optind << endl;
	    optind++;		// HACK: skip the unknown option's parameter.
				// What if there was no param?? - no problem,
				// next call to getopt will just pick up the
				// next option.
	    break;
	}
    }


    
    // need one more param, the circuit file name
    if (optind >= argc) {
	usage(argv);
	exit (EXIT_SUCCESS);
    }

    clog << "optind = " << optind
	 << ", using circuit file " << argv[optind] << endl;

    gates_in.open  (argv[optind]);

    if (!gates_in) {
	cerr << "Failed to open circuit file " << argv[optind] << endl;
	exit (EXIT_FAILURE);
    }
    

    
    // reset the getopt loop, before redoing the option parsing in do_configs()
    optind = 0;

    init_default_configs ();
    if ( do_configs (argc, argv) != 0 ) {
	cerr << "Crytpo option parsing failed" << endl;
	usage (argv);
	exit (EXIT_SUCCESS);
    }

    
    // initialize crypto
    try
    {
	g_provfact = init_crypt (g_configs);
    }
    catch (const crypto_exception& ex)
    {
	cerr << "Error making crypto providers: " << ex.what() << endl;
	exit (EXIT_FAILURE);
    }


    // and prepare the circuit and any input array containers.
    try {
	num_gates = prepare_gates_container (gates_in, g_configs.cct_name);
    }
    catch (exception & ex) {
	cerr << "Exception! " << ex.what() << endl;
	exit (1);
    }
    
}


const size_t CONTAINER_OBJ_SIZE = 128;


int prepare_gates_container (istream & gates_in,
			     const string& cct_name)
    throw (io_exception, std::exception)
{

    string line;
    unsigned gate_num = 0;
    unsigned max_gate = 0;
    ostringstream gate;
    vector< pair<index_t,string> > gates;

    ByteBuffer zeros (CONTAINER_OBJ_SIZE);
    zeros.set (0);

    string
	cct_cont    = cct_name + DIRSEP + CCT_CONT,
	gates_cont  = cct_name + DIRSEP + GATES_CONT,
	values_cont = cct_name + DIRSEP + VALUES_CONT;


    // read in all the gates
    while (true) {
	    
	// get the gate number
	if (gates_in >> gate_num) {
	    gate << gate_num << endl;
	    max_gate = max (max_gate, gate_num);
	    
	    // finish off this line
	    getline (gates_in, line);
	    clog << "gate " << gate_num << endl;
	}
	else if (gates_in.eof()) {
	    break;		// done
	}
	else {
	    // probably just a blank line
	    continue;
	}
	    
	/// and the rest of the gate lines
	while (getline (gates_in, line) && line != "") {
	    clog << "line: " << line << endl;
	    gate << line << endl;
	}

	clog << "gate done" << endl;

	gates.push_back ( make_pair (gate_num, gate.str()) );
	    
	gate.str("");
    }
	
    clog << "Done reading circuit" << endl;
    


    // create and fill in the containers
    
    FlatIO
	io_cct	    (cct_cont,
		     Just (make_pair (gates.size(), CONTAINER_OBJ_SIZE))),
	io_gates    (gates_cont,
		     Just (make_pair (max_gate+1, CONTAINER_OBJ_SIZE))),
	io_values   (values_cont,
		     Just (make_pair (max_gate+1, CONTAINER_OBJ_SIZE)));
    
    clog << "cct_cont size=" << gates.size() <<
	"; gates_cont size=" << max_gate + 1 << endl;

    
    index_t i = 0;
    FOREACH (g, gates) {
	gate_t gate = unserialize_gate (g->second);

	if (gate.op.kind == gate_t::Input) {
	    // get the input
	    switch (gate.typ.kind)
	    {
	    case gate_t::Scalar:
	    {
		int in;
		
		cout << "Need input " << gate.comment
		     << " for gate " << gate.num << ": " << flush;
		cin >> in;
		
		ByteBuffer val = optBasic2bb<int> (in);
		
		clog << "writing " << sizeof(in) << " byte input value" << endl;
		io_values.write (gate.num, val);
	    }
	    break;

	    case gate_t::Array:
	    {
		unsigned length	= gate.typ.params[0],
		    elem_size	= gate.typ.params[1];

		// NOTE: use the gate comment for the array's name, the runtime
		// has to do the same.
		string arr_cont_name = gate.comment;

		string in_str;
		vector<int> ins;
		
		// create the Array object to use to write
		Array arr (arr_cont_name,
			   Just (make_pair (length, elem_size)),
			   g_provfact.get());

		cerr << "Input array \"" <<  gate.comment << "\", elem size " << elem_size
		     << endl;

		// and prompt for all the values and write them in there
		for (int l_i = 0; l_i < length; l_i++)
		{

		    // ASSUME: the array elements are all 32-bit integers. If
		    // they're not, won't be prompting and populating the values
		    // correctly here.
		    for (int e_i = 0; e_i < elem_size / sizeof(int); e_i++)
		    {
			cerr << "Elt " << l_i << " field " << e_i << ": " << flush;
			do
			{
			    cin  >> in_str;
			} while (in_str.size() == 0 || in_str[0] == '#');

			ins.push_back (atoi (in_str.c_str()));
		    }

		    ByteBuffer ins_buf (elem_size);
		    for (int i=0; i < elem_size / sizeof(int); i++)
		    {
			memcpy (ins_buf.data() + i*sizeof(int),
				& ins[i],
				sizeof(ins[i]));
		    }

		    // write the array value into the array container
		    arr.write_clear (l_i, 0, ins_buf);
		}

		// and write a blank value of the right size into the values
		// table, the runtime will fill in the actual value.
                io_values.write (gate.num,
				 ByteBuffer (sizeof(ArrayHandle::des_t)));
		
	    } // end case Array:
	    break;

	    } // end switch (gate.typ.kind)

	} // end if (gate.op.kind == gate_t::Input)
	else {
	    // we should enter something into the values container
	    io_values.write (gate.num, zeros);
	}
	
	// write the string form of the gate into the two containers.
	ByteBuffer gatestring = ByteBuffer (g->second, ByteBuffer::SHALLOW);

	io_gates.write (g->first, gatestring);
	io_cct.write   (i++,      gatestring);

    } // end FOREACH (g, gates)
	
    return max_gate;
}
