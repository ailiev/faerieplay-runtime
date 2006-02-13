#include <string>
#include <list>
#include <functional>
#include <stdexcept>
#include <memory>

#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>

#include <math.h>

#include <boost/optional/optional.hpp>
#include <boost/none.hpp>
#include <boost/preprocessor/facilities/expand.hpp>

#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/io_filter.h>
#include <pir/card/io_filter_encrypt.h>
#include <pir/card/4758_sym_crypto.h>

#include <pir/card/configs.h>

#include <pir/common/comm_types.h>
#include <pir/common/logging.h>
#include <pir/common/sym_crypto.h>

#include <common/gate.h>
#include <common/misc.h>
#include <common/consts-sfdl.h>

#include "array.h"

#include "run-circuit.h"

// debug:
// myvalgrind.sh --tool=memcheck --num-callers=18 /home/sasho/work/code/sfdl-runtime/card/runtime -n v0

using namespace std;


auto_ptr<CryptoProviderFactory> g_provfact;




static void exception_exit (const std::exception& ex, const string& msg) {
    cerr << msg << ":" << endl
	 << ex.what() << endl
	 << "Exiting ..." << endl;
    exit (EXIT_FAILURE);
}

#include <signal.h>		// for raise()
void out_of_memory () {
    cerr << "Out of memory!" << endl;
    // trigger a SEGV, so we can get a core dump	    
    // * ((int*) (0x0)) = 42;
    raise (SIGTRAP);
    throw std::bad_alloc();
}




static void usage (const char* progname) {
    cerr << "Usage: " << progname << " [-p <card server por>]" << endl
	 << "\t[-d <crypto dir>]" << endl
	 << "\t[-c (use 4758 crypto hw?)]" << endl
	 << "\t[-n <circuit name>]" << endl
	 << "Runs circuit that card_server is accessing" << endl;
}




int main (int argc, char *argv[]) {

    set_new_handler (out_of_memory);

    init_default_configs ();
    if ( do_configs (argc, argv) != 0 )
    {
	usage (argv[0]);
	exit (EXIT_SUCCESS);
    }

//    try {
	g_provfact = init_crypt (g_configs);

	CircuitEval evaluator (g_configs.cct_name, g_provfact.get());

	LOG (Log::PROGRESS, CircuitEval::logger, "run-circuit starting circuit evaluation at "
	     << epoch_time);

	evaluator.eval ();

	LOG (Log::PROGRESS, CircuitEval::logger,
	     "run-circuit done with circuit evaluation at "
	     << epoch_time);
//     }
//     catch (const std::exception & ex) {
// 	cerr << "Exception: " << ex.what() << endl;
// 	exit (EXIT_FAILURE);
//     }
    
}





//
//
// class CircuitEval
//
//


using boost::shared_ptr;

using boost::optional;
using boost::none;


// static instantiations
Log::logger_t CircuitEval::logger, CircuitEval::gate_logger;

INSTANTIATE_STATIC_INIT(CircuitEval);



CircuitEval::CircuitEval (const std::string& cctname,
			  CryptoProviderFactory * fact)
    // tell the HostIO to not use a write cache (size 0)
    : _cct_io	(cctname + DIRSEP + CCT_CONT, none),
      _vals_io	(cctname + DIRSEP + VALUES_CONT, none)
{
    // NOTE: how are the keys set up? _vals_io calls initExisting() on the
    // filter, which then reads in the container keys using its #master pointer
    // (the FlatIO object).
    _vals_io.appendFilter (auto_ptr<HostIOFilter>
			   (new IOFilterEncrypt (&_vals_io,
						 shared_ptr<SymWrapper> (
						     new SymWrapper (fact)))));

    _cct_io.appendFilter (auto_ptr<HostIOFilter>
			  (new IOFilterEncrypt (&_cct_io,
						shared_ptr<SymWrapper> (
						    new SymWrapper (fact)))));
}

void CircuitEval::eval ()
{
    size_t num_gates = _cct_io.getLen();
    gate_t gate;
    
    for (unsigned i=0; i < num_gates; i++) {

	if (i % 100 == 0) {
	    LOG (Log::PROGRESS, logger, "Doing gate " << i);
	}

	read_gate (gate, i);

	LOG (Log::DEBUG, logger, gate << LOG_ENDL);

	do_gate (gate);
    }
}    


void CircuitEval::read_gate (gate_t & o_gate,
			     int gate_num)
    
{
    ByteBuffer gate_bytes;
    
    _cct_io.read (static_cast<index_t>(gate_num), gate_bytes);

    LOG (Log::DUMP, logger,
	 "Unwrapped gate to " << gate_bytes.len() << " bytes");
    string gate_str (gate_bytes.cdata(), gate_bytes.len());

    LOG (Log::DUMP, logger, "gate_byte len: " << gate_bytes.len());

    o_gate = unserialize_gate (gate_str);
}


void CircuitEval::do_gate (const gate_t& g)
{
    int res = 12345678;
    ByteBuffer res_bytes;


    switch (g.op.kind) {

    case gate_t::BinOp:
    {
	int arg_val[2];
	
	assert (g.inputs.size() == 2);
	for (int i=0; i < 2; i++) {
	    arg_val[i] = get_int_val (g.inputs[i]);
	}

	res = do_bin_op (static_cast<gate_t::binop_t>(g.op.params[0]),
			 arg_val[0], arg_val[1]);

	LOG (Log::DUMP, logger,
	     "BinOp returns " << res);

	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::SHALLOW);

    }
    break;

    
    case gate_t::UnOp:
    {
	int arg_gate, arg_val;
	
	assert (g.inputs.size() == 1);
	arg_gate = g.inputs[0];
	arg_val = get_int_val (arg_gate);

	res = do_un_op (static_cast<gate_t::unop_t>(g.op.params[0]),
			arg_val);
	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::SHALLOW);
	
    }
    break;

    
    case gate_t::Input:
	// already been done for us, go on
	break;

    case gate_t::Lit:
	// place the lit value into the slot
	res = g.op.params[0];
	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::SHALLOW);

	break;


    case gate_t::Select:
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
    break;

    
    case gate_t::ReadDynArray:
    {
	ByteBuffer arr_ptr = get_gate_val (g.inputs[0]);
	int idx = get_int_val (g.inputs[1]);
	
	ByteBuffer read_res = do_read_array (arr_ptr, idx);

	ByteBuffer outs [] = {  arr_ptr, read_res };
	res_bytes = concat_bufs (outs, outs + ARRLEN(outs));

	LOG (Log::DEBUG, logger,
	     "ReadDynArray gate returning " << res_bytes);
    }
    break;

    case gate_t::WriteDynArray:
    {
	int off = g.op.params[0];
	int len = g.op.params[1];

	int arr_gate_num = g.inputs[0];

	ByteBuffer arr_ptr = get_gate_val (arr_gate_num);
	// the index to write to
	int idx = get_int_val (g.inputs[1]);

	// and load up the rest of the inputs into vals
	vector<ByteBuffer> vals (g.inputs.size()-2);
	// what a PITA to call a member function!
	transform (g.inputs.begin()+2,
		   g.inputs.end(),
		   vals.begin(),
		   bind1st (mem_fun (&CircuitEval::get_gate_val),
			    this));

	// concatenate the inputs
	ByteBuffer ins = concat_bufs (vals.begin(), vals.end());

	// read in the whole array gate to see what depth it was at
	gate_t arr_gate;
	read_gate (arr_gate, arr_gate_num);

	do_write_array (arr_ptr,
			off,
			len >= 0 ? Just((size_t)len) : none,			
			idx,
			ins,
			arr_gate.depth, g.depth);

	// return the array pointer
	res_bytes = arr_ptr;
    }
    break;


    case gate_t::Slicer:
    {
	int off, len;

	off = g.op.params[0];
	len = g.op.params[1];

	ByteBuffer val = get_gate_val (g.inputs[0]);
	ByteBuffer out (len);

	assert (val.len() >= off + len);
	memcpy (out.data(), val.data() + off, len);

	res_bytes = out;
	
	LOG (Log::DEBUG, logger,
	     "Slicer gate returning " << res_bytes);
    }
    break;

    
    case gate_t::InitDynArray:
    {
	size_t elem_size, len;
	elem_size = g.op.params[0];
	len =	    g.op.params[1];

	// create a new array, give it a number and add it to the map (done
	// internally by newArray), and write the number as the gate value
	Array::des_t arr_num = Array::newArray (g.comment,
						len, elem_size,
						g_provfact.get());

	res_bytes = ByteBuffer (&arr_num, sizeof(arr_num), ByteBuffer::DEEP);
    }
    break;
    
	
    default:
	LOG (Log::ERROR, logger, "At gate " << g.num
	     << ", unknown operation " << g.op.kind);
	exit (EXIT_FAILURE);

    }

    LOG (Log::DEBUG, gate_logger,
	 setiosflags(ios::left)
	 << setw(6) << g.num
	 << setw(12) << res
	 << res_bytes);
    
    if (res_bytes.len() > 0) {
	put_gate_val (g.num, res_bytes);
    }
    

    if (elem (gate_t::Output, g.flags)) {
	int intval;
	assert (res_bytes.len() == sizeof(intval));
	memcpy (&intval, res_bytes.data(), sizeof(intval));
	cout << "Output " << g.comment << ": " << intval << endl;
    }
}




void CircuitEval::put_gate_val (int gate_num, const ByteBuffer& val)
{
    _vals_io.write (static_cast<index_t>(gate_num), val);
}


ByteBuffer CircuitEval::get_gate_val (int gate_num)
{
    ByteBuffer buf;
    
    _vals_io.read (static_cast<index_t>(gate_num), buf);

    LOG (Log::DUMP, logger, "get_gate_val for gate " << gate_num
	  << ": len=" << buf.len());
    
    return buf;
}


// get the current value at this gate's output
int CircuitEval::get_int_val (int gate_num)
{

    int answer;
    
    ByteBuffer buf = get_gate_val (gate_num);

    assert (buf.len() == sizeof(answer));
    
    memcpy (&answer, buf.data(), sizeof(answer));

    LOG (Log::DUMP, logger, "get_int_val (" << gate_num << ") ->"
	 << answer);

    return answer;
}


string CircuitEval::get_string_val (int gate_num)
{

    ByteBuffer buf = get_gate_val (gate_num);

    return string (buf.cdata(), buf.len());
}


ByteBuffer CircuitEval::do_read_array (const ByteBuffer& arr_ptr,
				       index_t idx)
{
    Array::des_t desc;

    assert (arr_ptr.len() == sizeof(desc));
    memcpy (&desc, arr_ptr.data(), sizeof(desc));

    Array & arr = Array::getArray (desc);

    return arr.read (idx);
}


void CircuitEval::do_write_array (const ByteBuffer& arr_ptr_buf,
				  size_t off,
				  optional<size_t> len,
				  index_t idx,
				  const ByteBuffer& new_val,
				  int prev_depth, int this_depth)
    
{
    Array::des_t arr_ptr;

    if (len)
    {
	assert (*len == new_val.len());
    }
    
    assert (arr_ptr_buf.len() == sizeof(arr_ptr));
    memcpy (&arr_ptr, arr_ptr_buf.data(), sizeof(arr_ptr));

    Array & arr = Array::getArray (arr_ptr);
    arr.write (idx, off, new_val);
}
