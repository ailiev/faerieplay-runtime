#include <string>
#include <list>
#include <functional>

#include <iostream>
#include <sstream>
#include <stdexcept>

#include <pir/card/hostcall.h>
#include <pir/common/comm_types.h>

#include <common/gate.h>

using namespace std;




void do_gate (const gate_t& gate)
    throw (xdr_exception, host_exception, comm_exception);


int get_int_val (int gate_num)
    throw (host_exception, xdr_exception, comm_exception);
string get_string_val (int gate_num)
    throw (host_exception, xdr_exception, comm_exception);
ByteBuffer get_gate_val (int gate_num)
    throw (host_exception, xdr_exception, comm_exception);

void put_gate_val (int gate_num, const ByteBuffer& val)
    throw (host_exception, xdr_exception, comm_exception);



int main (int argc, char *argv[]) {

    
    ByteBuffer gate_bytes, val;
    string gate_str;
    gate_t gate;

    object_name_t obj_name;
    

    try {
	for (int i=0; i < 17; i++) {

	    obj_name = object_name_t (object_id (i));
	    host_read_blob (CCT_CONT,
			    obj_name,
			    gate_bytes);

// 	clog << "gate_byte len: " << gate_bytes.len() << endl;
// 	    "bytes: " << gate_bytes.cdata() <<  endl;

	    gate = unserialize_gate (string(gate_bytes.cdata(),
					    gate_bytes.len()));

	    print_gate (cout, gate);

	    do_gate (gate);

	}
    }
    catch (const std::exception & ex) {
	cerr << "Exception: " << ex.what() << endl;
	exit (EXIT_FAILURE);
    }
    


}



void do_gate (const gate_t& g)
    throw (xdr_exception, host_exception, comm_exception)
{
    int res;
    ByteBuffer res_bytes;


    switch (g.op.kind) {

    case BinOp:
    {
	int arg_val[2];
	
	assert (g.inputs.size() == 2);
	for (int i=0; i < 2; i++) {
	    arg_val[i] = get_int_val (g.inputs[i]);
	}

	res = do_bin_op (static_cast<binop_t>(g.op.param1),
			 arg_val[0], arg_val[1]);

	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::no_free);

    }
    break;

    
    case UnOp:
    {
	int arg_gate, arg_val;
	
	assert (g.inputs.size() == 1);
	arg_gate = g.inputs[0];
	arg_val = get_int_val (arg_gate);

	res = do_un_op (static_cast<unop_t>(g.op.param1),
			arg_val);
	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::no_free);
	
    }
    break;

    
    case Input:
	// already been done for us, go on
	break;

    case Lit:
	// place the lit value into the slot
	res = g.op.param1;
	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::no_free);

	break;


    case Select:
    {
	ByteBuffer input_vals[2];
	int selector;

	assert (g.inputs.size() == 3);
	selector = get_int_val (g.inputs[0]);
	for (int i=0; i < 2; i++) {
	    input_vals[i] = get_gate_val (g.inputs[i+1]);
	}

	// it is really a selector!
	res_bytes = selector ? input_vals[0] : input_vals[1];
    }	

/*    
    case ReadDynArray:
    {
	string arr_name = get_string_val (g.inputs[0]);
	int idx = get_int_val (g.inputs[1]);

//	res_bytes = do_read_array (arr_name, idx);
    }
    break;

    case WriteDynArray:
    {
	int off = g.op.param1;
	int len = g.op.param2;

	string arr_name = get_string_val (g.inputs[0]);
	int idx = get_int_val (g.inputs[1]);


	// and load up the rest of the inputs into vals
	vector<ByteBuffer> vals (g.inputs.size()-2);
	transform (g.inputs.begin()+2,
		   g.inputs.end(),
		   vals.begin(),
		   get_gate_val);

	// concatenate the inputs
	ByteBuffer ins = concat_bufs (vals);

	do_write_array (arr_name, idx, ins);
    }

*/

    }

    if (res_bytes.len() > 0) {
	put_gate_val (g.num, res_bytes);
    }
    

    if (elem (Output, g.flags)) {
	int intval;
	assert (res_bytes.len() >= sizeof(intval));
	memcpy (&intval, res_bytes.data(), sizeof(intval));
	clog << "Output g.comment: " << intval << endl;
    }
}


void put_gate_val (int gate_num, const ByteBuffer& val)
    throw (host_exception, xdr_exception, comm_exception)
{
    host_write_blob (VALUES_CONT,
		     object_name_t (object_id (gate_num)),
		     val);
}


ByteBuffer get_gate_val (int gate_num)
    throw (host_exception, xdr_exception, comm_exception)
{

    ByteBuffer buf;
    
    host_read_blob (VALUES_CONT,
		    object_name_t (object_id (gate_num)),
		    buf);

    clog << "get_gate_val for gate " << gate_num
	 << ": len=" << buf.len() << endl;
    
    return buf;
}


// get the current value at this gate's output
int get_int_val (int gate_num)
    throw (host_exception, xdr_exception, comm_exception)
{

    int answer;
    
    ByteBuffer buf = get_gate_val (gate_num);

    assert (buf.len() == sizeof(answer));
    
    memcpy (&answer, buf.data(), sizeof(answer));
    return answer;
}


string get_string_val (int gate_num)
    throw (host_exception, xdr_exception, comm_exception)
{

    ByteBuffer buf = get_gate_val (gate_num);

    return string (buf.cdata(), buf.len());
}


	
    

