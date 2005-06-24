#include <string>

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utility>

#include <pir/host/objio.h>
#include <pir/common/utils.h>

#include <common/consts.h>
#include <common/gate.h>


//#include <pir/common/comm_types.h>


using namespace std;


int prepare_gates_container (istream & gates_in)
    throw (io_exception, std::exception);

void prepare_values_container (int num_gates)
    throw (io_exception, std::logic_error);




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
	cerr << "Usage: " << argv[0] << " <circuit file>" << endl;
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
	    int in;

	    cerr << "Need input " << gate.comment
		 << " for gate " << gate.num << ": " << flush;
	    cin >> in;

	    ByteBuffer val (&in, sizeof(in), ByteBuffer::no_free);

	    clog << "writing " << sizeof(in) << " byte input value" << endl;
	    write_obj (val, VALUES_CONT, gate.num);
	}
	    
	write_obj (ByteBuffer (g->second, ByteBuffer::SHALLOW),
		   GATES_CONT,
		   g->first);

	write_obj (ByteBuffer (g->second, ByteBuffer::SHALLOW),
		   CCT_CONT,
		   i++);
    }
	
    return max_gate;
}
