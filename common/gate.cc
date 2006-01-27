#include <list>
#include <vector>
#include <string>
#include <ostream>
#include <sstream>
#include <iterator>

#include <pir/common/exceptions.h>

#include "gate.h"


using namespace std;


gate_t::gate_t ()
    // can't have this if we just use push_back to add inputs.
//   : inputs (2)
{}
    

int do_bin_op (gate_t::binop_t op, int x, int y) {

    switch (op) {
    case     gate_t::Plus:  return x + y;
    case     gate_t::Minus: return x - y;

	// NOTE: we need to be able to handle division by zero. producing an
	// invalid answer is fine, hopefully a subsequent gate_t::Select will ignore it.
	// TODO: should concoct a "bottom" value, to represent invalid
	// computation results which should not be used in subsequent
	// operations.
    case     gate_t::Div:   return y != 0 ? x / y : 0;
    case     gate_t::Mod:   return y != 0 ? x % y : 0;
    case     gate_t::Times: return x * y;
		
    case     gate_t::LT:    return x < y;
    case     gate_t::GT:    return x > y;
    case     gate_t::Eq:    return x == y;
    case     gate_t::LTEq:  return x <= y;
    case     gate_t::GTEq:  return x >= y;
    case     gate_t::NEq:   return x != y;
    case     gate_t::SR:   return x >> y;
    case     gate_t::SL:   return x << y;
    default: cerr << "unknown binop " << op << endl;
    }
     
}

int do_un_op (gate_t::unop_t op, int x) {
    switch (op) {
    case gate_t::Negate: return -x;
    case gate_t::BNot:   return ~x;
    case gate_t::LNot:   return !x;
    }
}




gate_t unserialize_gate (const string& gate)
    throw (io_exception)
{

    gate_t answer;
    string line, word;
    istringstream lines (gate);
    

//    cerr << "the gate " << gate << endl;
    
    lines >> answer.num;
    
    // finish off the first line
    getline (lines, line);

    //
    // flags
    //
    {
	getline (lines, line);
	istringstream line_str (line);
	
	while (line_str >> word) {
	    if (word == "Output") {
		answer.flags.push_back (gate_t::Output);
	    }
	}
    }
 
    //
    // return type
    //
    {
	getline (lines, line);
	istringstream line_str (line);
//	clog << "type line:" << line << endl;
	line_str >> word;
	if (word == "array") {
	    answer.typ.kind = gate_t::Array;
	    line_str >> answer.typ.params[0]; // array length
	    line_str >> answer.typ.params[1]; // size of element type
	}
	else if (word == "scalar") {
	    answer.typ.kind = gate_t::Scalar;
	}
	else {
	    cerr << "Unknown type " << word << endl;
	}
    }
    
    //
    // operation
    //

    {
	getline (lines, line);
	istringstream line_str (line);
//	clog << "op line:" << line << endl;
	line_str >> word;
	if (word == "BinOp") {
	    answer.op.kind = gate_t::BinOp;
	    line_str >> word;
	    if (word == "+") {
		answer.op.params[0] = gate_t::Plus;
	    }
	    else if (word == "-") {
		answer.op.params[0] = gate_t::Minus;
	    }
	    
	    else if (word == "*") {
		answer.op.params[0] = gate_t::Times;
	    }
	    else if (word == "/") {
		answer.op.params[0] = gate_t::Div;
	    }
	    else if (word == "%") {
		answer.op.params[0] = gate_t::Mod;
	    }

	    else if (word == "<") {
		answer.op.params[0] = gate_t::LT;
	    }
	    else if (word == ">") {
		answer.op.params[0] = gate_t::GT;
	    }
	    else if (word == "==") {
		answer.op.params[0] = gate_t::Eq;
	    }
	    else if (word == ">=") {
		answer.op.params[0] = gate_t::GTEq;
	    }
	    else if (word == "<=") {
		answer.op.params[0] = gate_t::LTEq;
	    }
	    else if (word == "!=") {
		answer.op.params[0] = gate_t::NEq;
	    }
	    else if (word == "<<") {
		answer.op.params[0] = gate_t::SL;
	    }
	    else if (word == ">>") {
		answer.op.params[0] = gate_t::SR;
	    }
	    else {
		cerr << "unknown BinOp " << word << endl;
	    }
	}
	else if (word == "UnOp") {
	    answer.op.kind = gate_t::UnOp;
	    line_str >> word;
	    if (word == "-") {
		answer.op.params[0] = gate_t::Negate;
	    }
	    else if (word == "!") {
		answer.op.params[0] = gate_t::LNot;
	    }
	    else if (word == "~") {
		answer.op.params[0] = gate_t::BNot;
	    }
	    else {
		cerr << "unknown UnOp " << word << endl;
	    }
	}
	else if (word == "Input") {
	    answer.op.kind = gate_t::Input;
	}
	else if (word == "Select") {
	    answer.op.kind = gate_t::Select;
	}
	else if (word == "Lit") {
	    answer.op.kind = gate_t::Lit;
	    line_str >> answer.op.params[0];
	}
	else if (word == "ReadDynArray") {
	    answer.op.kind = gate_t::ReadDynArray;
	}
	else if (word == "WriteDynArray") {
	    answer.op.kind = gate_t::WriteDynArray;
	    line_str >> answer.op.params[0]; // offset
	    line_str >> answer.op.params[1]; // length
	}
	else if (word == "Slicer") {
	    answer.op.kind = gate_t::Slicer;
	    line_str >> answer.op.params[0]; // offset
	    line_str >> answer.op.params[1]; // length
	
	}
	else if (word == "InitDynArray") {
	    answer.op.kind = gate_t::InitDynArray;
	    line_str >> answer.op.params[0]; // element size
	    line_str >> answer.op.params[1]; // array length
	}
	else {
	    cerr << "Unknown gate op " << word << endl;
	}
    }


    //
    // inputs
    //
    {
	getline (lines, line);
	istringstream line_str (line);
	int i;
	while (line_str >> i) {
	    answer.inputs.push_back (i);
	}
    }

    //
    // depth
    //
    {
	getline (lines, line);
	istringstream line_str (line);
	line_str >> answer.depth;
    }
    

    
    //
    // comment
    //
    getline (lines, answer.comment);

    return answer;
}


ostream& print_gate (ostream & out, const gate_t & g) {
    out << "num: " << g.num << endl;

    switch (g.typ.kind) {
    case gate_t::Array:
	out << "Array " << g.typ.params[0] << " " << g.typ.params[1] << endl;
	break;
    case gate_t::Scalar:
	out << "Scalar" << endl;
	break;
    }

    switch (g.op.kind) {
    case gate_t::BinOp:
	out << "BinOp ";
	switch (g.op.params[0]) {
	case gate_t::Plus:
	    out << "+" << endl;
	    break;
	case gate_t::Minus:
	    out << "-" << endl;
	    break;
	    
	case gate_t::Times:
	    out << "*" << endl;
	    break;
	case gate_t::Div:
	    out << "/" << endl;
	    break;
	case gate_t::Mod:
	    out << "%" << endl;
	    break;
	    
	case gate_t::LT:
	    out << "<" << endl;
	    break;
	case gate_t::GT:
	    out << ">" << endl;
	    break;
	    
	case gate_t::SR:
	    out << ">>" << endl;
	    break;
	case gate_t::SL:
	    out << "<<" << endl;
	    break;
	    
	default:
	    out << "unknown" << endl;
	    break;
	}
	break;
	
    case gate_t::UnOp:
	out << "UnOp ";
	switch (g.op.params[0]) {
	case gate_t::Negate:
	    out << "-" << endl;
	    break;
	case gate_t::BNot:
	    out << "~" << endl;
	    break;
	case gate_t::LNot:
	    out << "!" << endl;
	    break;
	    
	default:
	    out << "unknown " << g.op.params[0] << endl;
	    break;
	}
	break;

    case gate_t::ReadDynArray:
	out << "ReadDynArray" << endl;
	break;
    case gate_t::WriteDynArray:
	out << "WriteDynArray" << endl;
	break;
    case gate_t::Input:
	out << "Input" << endl;
	break;
    case gate_t::Select:
	out << "Select" << endl;
	break;
    case gate_t::Slicer:
	out << "Slicer" << endl;
	break;
    case gate_t::Lit:
	out << "Lit" << endl;
	break;

    default:
	out << "something else" << endl;
    }

    out  << "inputs: ";
    copy (g.inputs.begin(), g.inputs.end(),
	  ostream_iterator<int> (out, " "));
    out << endl;

    out << "flags: ";
    copy (g.flags.begin(), g.flags.end(),
	  ostream_iterator<gate_t::gate_flag_t> (out, " "));
    out << endl;

    out << "comm: ";
    out << g.comment << endl << endl;

    out << "depth: ";
    out << g.depth << endl;

    return out;
    
}

