#include <string>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utility>

#include <pir/host/objio.h>
#include <pir/common/utils.h>
#include <pir/common/consts.h>

#include <common/consts-sfdl.h>
#include <common/gate.h>

#include <common/misc.h>

//#include <pir/common/comm_types.h>


using namespace std;


int prepare_gates_container (istream & gates_in,
			     const string& cct_name)
    throw (io_exception, std::exception);


void usage (char *argv[]) {
    cerr << "Usage: " << argv[0] << " <circuit file> <circuit name>" << endl
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
    
    // shut up clog for now
    /*
    ofstream out("/dev/null");
    if (out)
	clog.rdbuf(out.rdbuf());
    */
    
    int opt;
    while ((opt = getopt(argc, argv, "d:")) != EOF) {
	switch (opt) {

	case 'd':		//directory for keys etc
	    store_root = optarg;
	    break;

	default:
	    usage(argv);
	    exit (EXIT_SUCCESS);
	}
    }

    ifstream gates_in;
    string cct_name;
    
    // need two more params
    if (optind + 1 >= argc) {
	usage(argv);
	exit (EXIT_SUCCESS);
    }

    gates_in.open  (argv[optind]);
    cct_name = argv[optind+1];

    if (!gates_in) {
	cerr << "Failed to open circuit file " << argv[optind] << endl;
	exit (EXIT_FAILURE);
    }
    
    
    init_objio (store_root);
    
    try {
	num_gates = prepare_gates_container (gates_in, cct_name);
    }
    catch (exception & ex) {
	cerr << "Exception! " << ex.what() << endl;
	exit (1);
    }
    
}


const int CONTAINER_OBJ_SIZE = 128;


int prepare_gates_container (istream & gates_in,
			     const string& cct_name)
    throw (io_exception, std::exception)
{

    string line;
    int gate_num = 0;
    int max_gate = 0;
    ostringstream gate;
    vector< pair<int,string> > gates;

    ByteBuffer zeros (CONTAINER_OBJ_SIZE);
    zeros.set (0);

    string
	cct_cont    = cct_name + DIRSEP + CCT_CONT,
	gates_cont  = cct_name + DIRSEP + GATES_CONT,
	values_cont = cct_name + DIRSEP + VALUES_CONT;


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

	gates.push_back ( pair<int,string> (gate_num, gate.str()) );
	    
	gate.str("");
    }
	
    clog << "Done reading circuit" << endl;
    
    init_obj_container (cct_cont,
			gates.size(),
			CONTAINER_OBJ_SIZE);
    init_obj_container (gates_cont,
			max_gate + 1,
			CONTAINER_OBJ_SIZE);
    init_obj_container (values_cont,
			max_gate + 1,
			CONTAINER_OBJ_SIZE);
    
    clog << "cct_cont size=" << gates.size() <<
	"; gates_cont size=" << max_gate + 1 << endl;

    
    int i = 0;
    FOREACH (g, gates) {
	gate_t gate = unserialize_gate (g->second);

	if (gate.op.kind == gate_t::Input) {
	    // get the input
	    switch (gate.typ.kind) {
	    case gate_t::Scalar:
	    {
		int in;
		
		cout << "Need input " << gate.comment
		     << " for gate " << gate.num << ": " << flush;
		cin >> in;
		
		ByteBuffer val (&in, sizeof(in), ByteBuffer::SHALLOW);
		
		clog << "writing " << sizeof(in) << " byte input value" << endl;
		write_obj (val, values_cont, gate.num);
	    }
	    break;

	    case gate_t::Array:
	    {
		int length	= gate.typ.params[0],
		    elem_size	= gate.typ.params[1];

		char arr_name = 'A';
		arr_ptr_t arr_ptr = make_array_pointer (arr_name, 0);
		string arr_cont_name = make_array_container_name (arr_ptr);

		string in_str;
		vector<int> ins (elem_size);
		
		// create the clear-text array container
		init_obj_container ( arr_cont_name,
				     length,
				     elem_size * 4 );

		// and prompt for all the values and write them in there
		for (int l_i = 0; l_i < length; l_i++) {

		    for (int e_i = 0; e_i < elem_size; e_i++) {
			cerr << "Elt " << l_i << " field " << e_i << ": " << flush;
			do {
			    cin  >> in_str;
			} while (in_str.size() == 0 || in_str[0] == '#');

			ins[e_i] = atoi (in_str.c_str());
		    }

		    ByteBuffer ins_buf (elem_size * 4);
		    for (int i=0; i<elem_size; i++) {
			memcpy (ins_buf.data() + (i*4), & ins[i], sizeof(ins[i]));
		    }

		    // write the array value into the array container
		    write_obj (ins_buf, arr_cont_name,
			       l_i);

		}

		// and write the array pointer into the values table
		write_obj (ByteBuffer (&arr_ptr, sizeof(arr_ptr), ByteBuffer::SHALLOW),
			   values_cont,
			   gate.num);
		
	    } // end case Array:
	    break;

	    } // end switch (gate.typ.kind)

	} // end if (gate.op.kind == gate_t::Input)
	else {
	    // we should enter something into the values container
	    write_obj (zeros,
		       values_cont,
		       gate.num);
	}
	
	// write the string form of the gate into the two containers.
	ByteBuffer gatestring = ByteBuffer (g->second, ByteBuffer::SHALLOW);

	write_obj (gatestring,
		   gates_cont,
		   g->first);

	write_obj (gatestring,
		   cct_cont,
		   i++);
    } // end FOREACH (g, gates)
	
    return max_gate;
}
