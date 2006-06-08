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


std::auto_ptr<CryptoProviderFactory> g_provfact;




static void exception_exit (const std::exception& ex, const std::string& msg) {
    using namespace std;
    
    cerr << msg << ":" << endl
	 << ex.what() << endl
	 << "Exiting ..." << endl;
    exit (EXIT_FAILURE);
}

#include <signal.h>		// for raise()
void out_of_memory () {
    using namespace std;
    
    cerr << "Out of memory!" << endl;
    // trigger a SEGV, so we can get a core dump	    
    // * ((int*) (0x0)) = 42;
    raise (SIGTRAP);
    throw std::bad_alloc();
}




static void usage (const char* progname) {
    using namespace std;
    
    cerr << "Usage: " << progname << " [-p <card server por>]" << endl
	 << "\t[-d <crypto dir>]" << endl
	 << "\t[-c (use 4758 crypto hw?)]" << endl
	 << "\t[-n <circuit name>]" << endl
	 << "Runs circuit that card_server is accessing" << endl;
}




int main (int argc, char *argv[]) {

    using namespace std;
    using namespace pir;
    
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

	clog << "run-circuit starting circuit evaluation at "
	     << epoch_time <<  endl;
	
	evaluator.eval ();
	
	clog << "run-circuit done with circuit evaluation at "
	     << epoch_time << endl;

//     }
//     catch (const std::exception & ex) {
// 	cerr << "Exception: " << ex.what() << endl;
// 	exit (EXIT_FAILURE);
//     }
    
}




OPEN_NS

//
//
// class CircuitEval
//
//

using std::auto_ptr;
using std::string;
using std::vector;

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
      _vals_io	(cctname + DIRSEP + VALUES_CONT, none),
      _prov_fact    (fact)
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
    optional<int> res = 12345678;
    ByteBuffer res_bytes;


    switch (g.op.kind) {

    case gate_t::BinOp:
    {
	optional<int> arg_val[2];
	
	assert (g.inputs.size() == 2);
	for (int i=0; i < 2; i++) {
	    arg_val[i] = get_int_val (g.inputs[i]);
	}

	res = do_bin_op (static_cast<gate_t::binop_t>(g.op.params[0]),
			 arg_val[0], arg_val[1]);

	LOG (Log::DUMP, logger,
	     "BinOp returns " << res);

	res_bytes = optBasic2bb (res);

    }
    break;

    
    case gate_t::UnOp:
    {
	int arg_gate;
	optional<int> arg_val;
	
	assert (g.inputs.size() == 1);
	arg_gate = g.inputs[0];
	arg_val = get_int_val (arg_gate);

	res = do_un_op (static_cast<gate_t::unop_t>(g.op.params[0]),
			arg_val);

	res_bytes = optBasic2bb (res);
	
    }
    break;

    
    case gate_t::Input:
	if (g.typ.kind == gate_t::Array)
	{
	    // need to load up the array
	    string arr_cont_name = g.comment;

	    ArrayHandle::des_t arr_ptr = ArrayHandle::newArray (arr_cont_name,
								_prov_fact,
								g.depth);

	    res_bytes = optBasic2bb (Just (arr_ptr));
	}

	// nothing to do for Scalars, the value is already set up by prep-circuit
	break;

    case gate_t::Lit:
	// place the lit value into the slot
	res = g.op.params[0];
	res_bytes = optBasic2bb (res);

	break;


    case gate_t::Select:
    {
	ByteBuffer input_vals[2];
	
	optional<int> selector;

	bool sel_first;

	assert (g.inputs.size() == 3);
	selector = get_int_val (g.inputs[0]);
	
	// treat a Nothing selector as False
	sel_first = ! (!selector || !*selector);
	    

	for (int i=0; i < 2; i++) {
	    input_vals[i] = get_gate_val (g.inputs[i+1]);
	}


	if (g.typ.kind == gate_t::Array)
	{
	    // this is a bit longer, as array selection involved reading through
	    // both arrays to hide which one is selected. hence handled by the
	    // Array code
	    
	    optional<ArrayHandle::des_t> descs [] = {
		bb2optBasic<ArrayHandle::des_t> (input_vals[0]),
		bb2optBasic<ArrayHandle::des_t> (input_vals[1])
	    };
	    
	    assert (("Cannot select on a Nothing array handle",
		     descs[0] && descs[1]));
	    
	    ArrayHandle arrs[] = { ArrayHandle::getArray (*descs[0]),
				   ArrayHandle::getArray (*descs[1]) };

	    ArrayHandle result = ArrayHandle::select (
		arrs[0], arrs[1],
		selector,
		g.depth);

	    res_bytes = optBasic2bb (Just (result.getDescriptor()));
	    
	}
	else if (g.typ.kind == gate_t::Scalar)
	{
	    // simple!
	    res_bytes = sel_first  ? input_vals[0] : input_vals[1];
	}
    }	
    break;

    
    case gate_t::ReadDynArray:
    {
	ByteBuffer arr_ptr = get_gate_val (g.inputs[0]);
	optional<int> idx = get_int_val (g.inputs[1]);
	
	ByteBuffer val;
	ByteBuffer arr2 = do_read_array (arr_ptr, idx, g.depth, val);

	ByteBuffer outs [] = {  arr2, val };
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
	optional<int> idx = get_int_val (g.inputs[1]);

	// and load up the rest of the inputs into vals
	vector<ByteBuffer> vals (g.inputs.size()-2);
	// what a PITA to call a member function through STL algrithms...
	transform (g.inputs.begin()+2,
		   g.inputs.end(),
		   vals.begin(),
		   std::bind1st (std::mem_fun (&CircuitEval::get_gate_val),
				 this));

	// concatenate the inputs
	ByteBuffer ins = concat_bufs (vals.begin(), vals.end());

	// read in the whole array gate to see what depth it was at
	gate_t arr_gate;
	read_gate (arr_gate, arr_gate_num);

	ByteBuffer arr_desc2 = do_write_array (
	    arr_ptr,
	    off,
	    len >= 0 ? Just((size_t)len) : none,			
	    idx,
	    ins,
	    arr_gate.depth, g.depth);

	// return the array pointer
	res_bytes = arr_desc2;
    }
    break;


    case gate_t::Slicer:
    {
	int off, len;

	off = g.op.params[0];
	len = g.op.params[1];

	// HACK: here we're making a ByteBuffer representing an opaque
	// optional<> value of the right length, as manipulated by optBasic2bb()
	// and bb2optBasic().

	ByteBuffer val = get_gate_val (g.inputs[0]);
	ByteBuffer out (len);

	LOG (Log::DEBUG, logger,
	     "Slicer (" << off << "," << len << ")" << val);

	if (!isOptBBJust (val))
	{
	    out.data()[0] = 0;
	}
	else
	{
	    // last index we need to read is (off+1)+(len-1)-1 = off+len-1,
	    // so need	val.len() > off+len-1
	    // or	val.len() >= off+len
	    assert (val.len() >= off + len);
	    
	    out.data()[0] = 1;
	    memcpy (out.data() + 1,
		    val.data() + off + 1, // + 1 so we skip the Just indicator
					  // byte
		    len-1	// - 1: same reason
		);

	    LOG (Log::DEBUG, logger,
		 "Slicer returns " << res_bytes);
	}

	res_bytes = out;
    }
    break;

    
    case gate_t::InitDynArray:
    {
	size_t elem_size, len;
	elem_size = g.op.params[0];
	len =	    g.op.params[1];

	// create a new array, give it a number and add it to the map (done
	// internally by newArray), and write the number as the gate value
	ArrayHandle::des_t arr_desc = ArrayHandle::newArray (g.comment,
							     len, elem_size,
							     g_provfact.get(),
							     g.depth);

	
	res_bytes = optBasic2bb (Just (arr_desc));
    }
    break;
    
	
    default:
	LOG (Log::ERROR, logger, "At gate " << g.num
	     << ", unknown operation " << g.op.kind);
	exit (EXIT_FAILURE);

    }

    LOG (Log::DEBUG, gate_logger,
	 std::setiosflags(std::ios::left)
	 << std::setw(6) << g.num
	 << std::setw(12) << res
	 << res_bytes);
    
    if (res_bytes.len() > 0) {
	put_gate_val (g.num, res_bytes);
    }
    

    if (elem (gate_t::Output, g.flags)) {
	optional<int> intval = bb2optBasic<int> (res_bytes);

	std::cout << "Output " << g.comment << ": ";
	if (intval)
	{
	    std::cout << *intval;
	}
	else
	{
	    std::cout << "Nothing";
	}
	std::cout << std::endl;
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
optional<int> CircuitEval::get_int_val (int gate_num)
{

    optional<int> answer;
    
    ByteBuffer buf = get_gate_val (gate_num);

    answer = bb2optBasic<int> (buf);
    
    LOG (Log::DUMP, logger, "get_int_val (" << gate_num << ") ->"
	 << answer);

    return answer;
}


#if 0
string CircuitEval::get_string_val (int gate_num)
{

    ByteBuffer buf = get_gate_val (gate_num);

    return string (buf.cdata(), buf.len());
}
#endif


ByteBuffer CircuitEval::do_read_array (const ByteBuffer& arr_ptr,
				       optional<int> idx,
				       unsigned depth,
				       ByteBuffer & o_val)
{
    optional<ArrayHandle::des_t> desc, null;

    desc = bb2optBasic<ArrayHandle::des_t> (arr_ptr);

    if (!desc)
    {
	LOG (Log::ERROR, logger,
	     "CircuitEval::do_read_array: got a null array pointer!");
	return optBasic2bb (null);
    }

    ArrayHandle & arr = ArrayHandle::getArray (*desc);

    if (!idx || *idx < 0 || *idx >= arr.length())
    {
	// no write
	LOG (Log::INFO, logger,
	     "do_read_array got a null or outside-bounds index " << idx);
	return arr_ptr;
    }

    ArrayHandle & arr2 = arr.read (*idx, o_val, depth);
    
    return optBasic2bb<ArrayHandle::des_t> (arr2.getDescriptor());
}


ByteBuffer CircuitEval::do_write_array (const ByteBuffer& arr_ptr_buf,
					size_t off,
					optional<size_t> len,
					optional<int> idx,
					const ByteBuffer& new_val,
					int prev_depth, int this_depth)
    
{
    optional<ArrayHandle::des_t> desc, null;
    
    desc = bb2optBasic<ArrayHandle::des_t> (arr_ptr_buf);;

    if (!desc) {
	LOG (Log::ERROR, logger,
	     "CircuitEval::do_write_array: got a null array pointer!");
	return optBasic2bb (null);
    }

//     assert (arr_ptr_buf.len() == sizeof(desc));
//     memcpy (&desc, arr_ptr_buf.data(), sizeof(desc));

    if (len)
    {
	assert (*len == new_val.len());
    }
    
    ArrayHandle & arr = ArrayHandle::getArray (*desc);

    if (!idx || *idx < 0 || *idx >= arr.length()) {
	// no write
	LOG (Log::INFO, logger,
	     "do_write_array got a null or outside-bounds index " << idx);
	return arr_ptr_buf;
    }
    
    ArrayHandle & arr2 = arr.write (*idx, off, new_val, this_depth);

    return optBasic2bb<ArrayHandle::des_t> (arr2.getDescriptor());
}




#if 0
template<class Archive>
void serialize(Archive & ar, optional<int> & g, const unsigned int version)
{
    
    byte isJust = g;

    ar & isJust;
    
    if (isJust) {
	ar & *g;
    }
}
#endif


CLOSE_NS
