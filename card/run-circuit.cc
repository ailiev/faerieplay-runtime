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


// local functions.
namespace {
    // print the value of a gate, whose result bytes are 'valbytes'
    std::string
    write_value (const gate_t& gate, const ByteBuffer& valbytes);

    // get an array handle from an array pointer
    pir::ArrayHandle & get_array (const ByteBuffer& arr_ptr_buf);

    Log::logger_t s_progress_logger = Log::makeLogger ("circuit-progress",
						       boost::none,
						       Just (Log::PROGRESS));

}



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
    // trigger a SEGV, so we can get a core dump - useful when running on the
    // 4758 when we can't debug interactively, but do get core dumps.	    
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
	 << "Runs named circuit (that card_server is accessing)" << endl;
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
    : _gates_io (cctname + DIRSEP + GATES_CONT, none),
      _cct_io	(cctname + DIRSEP + CCT_CONT, none),
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
	    LOG (Log::PROGRESS, s_progress_logger,
		 "Doing gate " << i << " @" << epoch_secs());
	}

	read_gate_at_step (gate, i);

	LOG (Log::DEBUG, logger, gate << LOG_ENDL);

	do_gate (gate);
    }
}    


void CircuitEval::read_gate_at_step (gate_t & o_gate,
				     int step_num)
    
{
    read_gate_helper (_cct_io, o_gate, step_num);
}

void CircuitEval::read_gate (gate_t & o_gate,
			     int gate_num)
    
{
    read_gate_helper (_gates_io, o_gate, gate_num);
}


void CircuitEval::read_gate_helper (FlatIO & io,
				    gate_t & o_gate,
				    int num)
{
    ByteBuffer gate_bytes;
    
    io.read (static_cast<index_t>(num), gate_bytes);

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
	switch (g.typ.kind)
	{
	case gate_t::Array:
	{
	    // need to load up the array
	    string arr_cont_name = g.comment;

	    ArrayHandle::des_t arr_ptr = ArrayHandle::newArray (arr_cont_name,
								_prov_fact);

	    // for the logging
	    res = arr_ptr;

	    res_bytes = optBasic2bb (Just (arr_ptr));
	}
	break;
	case gate_t::Scalar:
	    // nothing to do for Scalars, the value is already set up by
	    // prep-circuit
	    // but, to keep the execution trace complete, load up the value and put
	    // it into res_bytes
	    res_bytes = get_gate_val (g.num);

	    res = bb2optBasic<int> (res_bytes);

	    break;
	} // switch (g.typ.kind)
	
	break;			// case gate_t::Input

    case gate_t::Lit:
	// place the lit value into the slot
	res 	  = Just (g.op.params[0]);
	res_bytes = optBasic2bb (res);

	break;


    case gate_t::Select:
    {
	ByteBuffer input_vals[2];
	
	optional<int> selector;

	bool sel_first;

	assert (g.inputs.size() == 3);
	// we do not select on arrays now.
	assert (g.typ.kind != gate_t::Array);

	selector = get_int_val (g.inputs[0]);
	
	// treat a Nothing selector as False
	sel_first = ! (!selector || !*selector);
	    

	for (int i=0; i < 2; i++) {
	    input_vals[i] = get_gate_val (g.inputs[i+1]);
	}

	// simple!
	res_bytes = sel_first  ? input_vals[0] : input_vals[1];
    }	
    break;

    
    case gate_t::ReadDynArray:
    {
	optional<int> 	    enable_i= get_int_val (g.inputs[0]);
	ByteBuffer 	    arr_ptr = get_gate_val (g.inputs[1]);
	optional<index_t>   idx     = static_cast<optional<index_t> > (get_int_val (g.inputs[2]));

	bool enable = enable_i ? (*enable_i != 0) : false;
	
	ByteBuffer val;
	ByteBuffer arr2 = do_read_array (enable, arr_ptr, idx, val);

	ByteBuffer outs [] = { arr2, val };
	res_bytes = concat_bufs (outs, outs + ARRLEN(outs));

	LOG (Log::DEBUG, logger,
	     "ReadDynArray gate returning " << res_bytes);
    }
    break;

    case gate_t::WriteDynArray:
    {
	int off = g.op.params[0];
	int len = g.op.params[1];

	optional<int> enable_i = get_int_val (g.inputs[0]);
	ByteBuffer arr_ptr     = get_gate_val (g.inputs[1]);
	// the index to write to
	optional<index_t> idx = static_cast<optional<index_t> > (get_int_val (g.inputs[2]));

	// and load up the rest of the inputs into vals
	vector<ByteBuffer> vals (g.inputs.size()-3);
	// what a PITA to call a member function through STL algrithms...
	transform (g.inputs.begin()+3,
		   g.inputs.end(),
		   vals.begin(),
		   std::bind1st (std::mem_fun (&CircuitEval::get_gate_val),
				 this));

	bool enable = enable_i ? (*enable_i != 0) : false;

	// concatenate the inputs
	ByteBuffer ins = concat_bufs (vals.begin(), vals.end());

	ByteBuffer arr_desc2 = do_write_array (
	    enable,
	    arr_ptr,
	    off,
	    len >= 0 ? Just((size_t)len) : none,			
	    idx,
	    ins);

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
	out.set (0);	// so it's not uninitialized
	

	LOG (Log::DEBUG, logger,
	     "Slicer (" << off << "," << len << ")" << val);

	if (!isOptBBJust (val))
	{
	    makeOptBBNothing(out);
	}
	else
	{
	    // last index we need to read is (off+1)+(len-1)-1 = off+len-1,
	    // so need	val.len() > off+len-1
	    // or	val.len() >= off+len
	    LOG (Log::DEBUG, logger,
		 "Slicer has val.len()=" << val.len()
		 << ",off=" << off << ",len=" << len);

	    if (val.len() < off + len) {
		// not enough data, presumably because an array read returned
		// NIL (and thus only the array pointer and no data). return NIL.
		LOG (Log::INFO, logger, "Slicer returning NIL");
		makeOptBBNothing (out);
	    }
	    else
	    {
		makeOptBBJust (out,
			       val.data() + 1 + off, // + 1 to skip the Just
						     // indicator byte
			       len-1);		     // -1 for same reason.

		LOG (Log::DEBUG, logger,
		     "Slicer returns " << res_bytes);
	    }
	}

	res_bytes = out;
    }
    break;

    
    case gate_t::InitDynArray:
    {
	size_t elem_size, len;
	elem_size = g.op.params[0];
	len =	    g.op.params[1];

	LOG (Log::DEBUG, logger,
	     "InitDynArray len=" << len << ", elem_size=" << elem_size);
	
	// create a new array, give it a number and add it to the map (done
	// internally by newArray), and write the number as the gate value
	ArrayHandle::des_t arr_desc = ArrayHandle::newArray (g.comment,
							     len, elem_size,
							     g_provfact.get());

	
	res_bytes = optBasic2bb (Just (arr_desc));
    }
    break;
    
	
    default:
	LOG (Log::ERROR, logger, "At gate " << g.num
	     << ", unknown operation " << g.op.kind);
	exit (EXIT_FAILURE);

    }


#ifdef LOGVALS
    log_gate_value (g, res_bytes);
#endif
    

    if (res_bytes.len() > 0) {
	put_gate_val (g.num, res_bytes);
    }
    

    if (elem (gate_t::Output, g.flags)) {
	switch (g.typ.kind)
	{
	case gate_t::Scalar:
	{
	    optional<int> intval = bb2optBasic<int> (res_bytes);

	    std::cout << "Output Scalar " << g.comment << ": ";
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
	break;
	case gate_t::Array:
	{
	    optional<ArrayHandle::des_t> desc;
	    ByteBuffer buf;
	    optional<int> int_val;
	    
	    desc = bb2optBasic<ArrayHandle::des_t> (res_bytes);

	    if (!desc)
	    {
		LOG (Log::ERROR, logger,
		     "CircuitEval Output: got a null array pointer!");
		return;
	    }

	    ArrayHandle & arr = ArrayHandle::getArray (*desc);

	    std::cout << "Output Array " << g.comment
		      << " of " << arr.length() << " elements:" << std::endl;
	    for (unsigned i=0; i < arr.length(); i++)
	    {
		(void) arr.read (true, i, buf);
		// FIXME: this is converting to integers, not very general.
		int_val = bb2optBasic<int> (buf);
		if (!int_val) {
		    std::cout << "Nothing" << std::endl;
		}
		else {
		    std::cout << *int_val << std::endl;
		}
	    }
	    

	}
	break;
	} // end switch (g.typ.kind)
    }
}



#ifdef LOGVALS
void CircuitEval::log_gate_value (const gate_t& g, const ByteBuffer& val)
{
    //
    // log to the gate values log
    // 
    // NOTE: this should be kept synchronized with the formatting routines in
    // Runtime.hs in the compiler (printOuts inside formatRun, and showsVal), so
    // that a text diff can reveal where the traces diverge.
    LOG ( Log::DEBUG, gate_logger,
	  std::setiosflags(std::ios::left)
	  << std::setw(14) << g.num
//	 << std::setw(12) << (res ? itoa(*res) : "N")
	  << write_value (g, val) );
    
}
#endif // LOGVALS




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


ByteBuffer CircuitEval::do_read_array (bool enable,
				       const ByteBuffer& arr_ptr,
				       optional<index_t> idx,
				       ByteBuffer & o_val)
{
    optional<ArrayHandle::des_t> null;
    
    ArrayHandle & arr = get_array (arr_ptr);

    ArrayHandle & arr2 = arr.read (enable, idx, o_val);
    
    return optBasic2bb<ArrayHandle::des_t> (arr2.getDescriptor());
}


ByteBuffer CircuitEval::do_write_array (bool enable,
					const ByteBuffer& arr_ptr_buf,
					size_t off,
					optional<size_t> len,
					optional<index_t> idx,
					const ByteBuffer& new_val)
    
{
    optional<ArrayHandle::des_t> desc, null;
    
    ArrayHandle & arr = get_array (arr_ptr_buf);
    
    if (len)
    {
	assert (*len == new_val.len());
    }
    
    ArrayHandle & arr2 = arr.write (enable, idx, off, new_val);

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






OPEN_ANON_NS


#ifdef LOGVALS

std::string
write_value (const gate_t& g,
	     const ByteBuffer& valbytes)
{
    std::ostringstream os;

    const size_t OPT_ARRDESC_SIZE = OPT_BB_SIZE(pir::ArrayHandle::des_t);
    
    if (g.typ.kind == gate_t::Array ||
	g.op.kind == gate_t::ReadDynArray ||
	g.op.kind == gate_t::WriteDynArray)
    {
	// the first 5 bytes is the array pointer, and the rest are other
	// values.
	const ByteBuffer
	    arr_ptr_buf (valbytes, 0, OPT_ARRDESC_SIZE),
	    val_rest (valbytes,
		      OPT_ARRDESC_SIZE,
		      valbytes.len() - OPT_ARRDESC_SIZE);

	// do not use parens if it's just one value, the array.
// 	if (val_rest.len() > 0) {
// 	    os << "(";
// 	}

	pir::ArrayHandle& arr = get_array (arr_ptr_buf);
	os << arr;

	if (val_rest.len() > 0) {
	    os << "," << val_rest;
// 	    os << ")";
	}

    }
    else
    {
	// a scalar
	os << valbytes;
    }

    return os.str();
}

#endif // LOGVALS


pir::ArrayHandle & get_array (const ByteBuffer& arr_ptr_buf)
{
    boost::optional<pir::ArrayHandle::des_t> desc;
    desc = bb2optBasic<pir::ArrayHandle::des_t> (arr_ptr_buf);

    if (!desc) {
	throw bad_arg_exception
	    ("CircuitEval::get_array: got a null array pointer!");
    }

    return pir::ArrayHandle::getArray (*desc);
}

CLOSE_NS			// anonymous


    
