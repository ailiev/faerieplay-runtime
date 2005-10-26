#include <string>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utility>

#include <pir/host/objio.h>
#include <pir/common/utils.h>

#include <common/consts-sfdl.h>
#include <common/gate.h>

#include <common/misc.h>

//#include <pir/common/comm_types.h>


using namespace std;


int prepare_gates_container (istream & gates_in)
    throw (io_exception, std::exception);

void prepare_values_container (int num_gates)
    throw (io_exception, std::logic_error);


void usage (char *argv[]) {
    cerr << "Usage: " << argv[0] << " <circuit file>" << endl;
    
    cerr << "Produces files:" << endl;
    cerr << CCT_CONT << ": container with the circuit gates, in text encoding,\n"
	"\tand ordered topologically." << endl;
    cerr << GATES_CONT << ": container with the circuit gates, in text encoding,\n"
	"\tand with gate number g in cont[g]." << endl;
    cerr << VALUES_CONT << ": container with the circuit values,\n"
	"\tinitially blank except for the inputs" << endl;
}


int main (int argc, char *argv[]) {

    string cct_cont_name = "circuit";
    int num_gates;

    // shut up clog for now
    /*
    ofstream out("/dev/null");
    if (out)
	clog.rdbuf(out.rdbuf());
    */
    
    ifstream gates_in (argv[1]);
    if (!gates_in) {
	usage(argv);
	exit (EXIT_FAILURE);
    }
    
    
    try {
	num_gates = prepare_gates_container (gates_in);
    }
    catch (exception & ex) {
	cerr << "Exception! " << ex.what() << endl;
	exit (1);
    }
    
}


const int CONTAINER_OBJ_SIZE = 128;


int prepare_gates_container (istream & gates_in)
    throw (io_exception, std::exception)
{

    string line;
    int gate_num = 0;
    int max_gate = 0;
    ostringstream gate;
    vector< pair<int,string> > gates;
    
    while (true) {
	    
	// get the gate number
	if (gates_in >> gate_num) {
	    gate << gate_num << endl;
	    max_gate = max (max_gate, gate_num);
	    
	    // finish off this line
	    getline (gates_in, line);
	    clog << "gate " << gate_num << endl;
	}
	else {
	    break;		// done
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
	

    init_obj_container (CCT_CONT,
			gates.size(),
			CONTAINER_OBJ_SIZE);
    init_obj_container (GATES_CONT,
			max_gate + 1,
			CONTAINER_OBJ_SIZE);
    init_obj_container (VALUES_CONT,
			max_gate + 1,
			CONTAINER_OBJ_SIZE);
    
    clog << "CCT_CONT size=" << gates.size() <<
	"; GATES_CONT size=" << max_gate + 1 << endl;

    
    int i = 0;
    FOREACH (g, gates) {
	gate_t gate = unserialize_gate (g->second);

	if (gate.op.kind == Input) {
	    // get the input
	    switch (gate.typ.kind) {
	    case Scalar:
	    {
		int in;
		
		cerr << "Need input " << gate.comment
		     << " for gate " << gate.num << ": " << flush;
		cin >> in;
		
		ByteBuffer val (&in, sizeof(in), ByteBuffer::no_free);
		
		clog << "writing " << sizeof(in) << " byte input value" << endl;
		write_obj (val, VALUES_CONT, gate.num);
	    }
	    break;

	    case Array:
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
		write_obj (ByteBuffer (&arr_ptr, sizeof(arr_ptr), ByteBuffer::no_free),
			   VALUES_CONT,
			   gate.num);
		
	    } // end case Array:
	    break;

	    } // end switch (gate.typ.kind)

	} // end if (gate.op.kind == Input)
	    
	write_obj (ByteBuffer (g->second, ByteBuffer::SHALLOW),
		   GATES_CONT,
		   g->first);

	write_obj (ByteBuffer (g->second, ByteBuffer::SHALLOW),
		   CCT_CONT,
		   i++);
    } // end FOREACH (g, gates)
	
    return max_gate;
}
