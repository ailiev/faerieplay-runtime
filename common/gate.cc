#include <list>
#include <vector>
#include <string>
#include <ostream>
#include <sstream>
#include <iterator>

#include <pir/common/exceptions.h>

#include "gate.h"


using namespace std;


gate_t::gate_t () :
    inputs (2)
{}
    

int do_bin_op (binop_t op, int x, int y) {

    switch (op) {
    case     Plus:  return x + y;
    case     Minus: return x - y;

	// NOTE: we need to be able to handle division by zero. producing an
	// invalid answer is fine, hopefully a subsequent Select will ignore it.
	// TODO: should concoct a "bottom" value, to represent invalid
	// computation results which should not be used in subsequent
	// operations.
    case     Div:   return y != 0 ? x / y : 0;
    case     Mod:   return y != 0 ? x % y : 0;
    case     Times: return x * y;
		
    case     LT:    return x < y;
    case     GT:    return x > y;
    case     Eq:    return x == y;
    case     LTEq:  return x <= y;
    case     GTEq:  return x >= y;
    case     NEq:   return x != y;
    case     SR:   return x >> y;
    case     SL:   return x << y;
    default: cerr << "unknown binop " << op << endl;
    }
     
}

int do_un_op (unop_t op, int x) {
    switch (op) {
    case Negate: return -x;
    case BNot:   return ~x;
    case LNot:   return !x;
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
		answer.flags.push_back (Output);
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
	    answer.typ.kind = Array;
	    line_str >> answer.typ.params[0]; // array length
	    line_str >> answer.typ.params[1]; // size of element type
	}
	else if (word == "scalar") {
	    answer.typ.kind = Scalar;
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
	    answer.op.kind = BinOp;
	    line_str >> word;
	    if (word == "+") {
		answer.op.params[0] = Plus;
	    }
	    else if (word == "-") {
		answer.op.params[0] = Minus;
	    }
	    
	    else if (word == "*") {
		answer.op.params[0] = Times;
	    }
	    else if (word == "/") {
		answer.op.params[0] = Div;
	    }
	    else if (word == "%") {
		answer.op.params[0] = Mod;
	    }

	    else if (word == "<") {
		answer.op.params[0] = LT;
	    }
	    else if (word == ">") {
		answer.op.params[0] = GT;
	    }
	    else if (word == "==") {
		answer.op.params[0] = Eq;
	    }
	    else if (word == ">=") {
		answer.op.params[0] = GTEq;
	    }
	    else if (word == "<=") {
		answer.op.params[0] = LTEq;
	    }
	    else if (word == "!=") {
		answer.op.params[0] = NEq;
	    }
	    else if (word == "<<") {
		answer.op.params[0] = SL;
	    }
	    else if (word == ">>") {
		answer.op.params[0] = SR;
	    }
	    else {
		cerr << "unknown BinOp " << word << endl;
	    }
	}
	else if (word == "UnOp") {
	    answer.op.kind = UnOp;
	    line_str >> word;
	    if (word == "-") {
		answer.op.params[0] = Negate;
	    }
	    else if (word == "!") {
		answer.op.params[0] = LNot;
	    }
	    else if (word == "~") {
		answer.op.params[0] = BNot;
	    }
	    else {
		cerr << "unknown UnOp " << word << endl;
	    }
	}
	else if (word == "Input") {
	    answer.op.kind = Input;
	}
	else if (word == "Select") {
	    answer.op.kind = Select;
	}
	else if (word == "Lit") {
	    answer.op.kind = Lit;
	    line_str >> answer.op.params[0];
	}
	else if (word == "ReadDynArray") {
	    answer.op.kind = ReadDynArray;
	}
	else if (word == "WriteDynArray") {
	    answer.op.kind = WriteDynArray;
	    line_str >> answer.op.params[1]; // offset
	    line_str >> answer.op.params[2]; // length
	}
	else if (word == "Slicer") {
	    answer.op.kind = Slicer;
	    line_str >> answer.op.params[0]; // offset
	    line_str >> answer.op.params[1]; // length
	
	}
	else if (word == "InitDynArray") {
	    answer.op.kind = InitDynArray;
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
    case Array:
	out << "Array " << g.typ.params[0] << " " << g.typ.params[1] << endl;
	break;
    case Scalar:
	out << "Scalar" << endl;
	break;
    }

    switch (g.op.kind) {
    case BinOp:
	out << "BinOp ";
	switch (g.op.params[0]) {
	case Plus:
	    out << "+" << endl;
	    break;
	case Minus:
	    out << "-" << endl;
	    break;
	    
	case Times:
	    out << "*" << endl;
	    break;
	case Div:
	    out << "/" << endl;
	    break;
	case Mod:
	    out << "%" << endl;
	    break;
	    
	case LT:
	    out << "<" << endl;
	    break;
	case GT:
	    out << ">" << endl;
	    break;
	    
	case SR:
	    out << ">>" << endl;
	    break;
	case SL:
	    out << "<<" << endl;
	    break;
	    
	default:
	    out << "unknown" << endl;
	    break;
	}
	break;
	
    case UnOp:
	out << "UnOp ";
	switch (g.op.params[0]) {
	case Negate:
	    out << "-" << endl;
	    break;
	case BNot:
	    out << "~" << endl;
	    break;
	case LNot:
	    out << "!" << endl;
	    break;
	    
	default:
	    out << "unknown " << g.op.params[0] << endl;
	    break;
	}
	break;

    case ReadDynArray:
	out << "ReadDynArray" << endl;
	break;
    case WriteDynArray:
	out << "WriteDynArray" << endl;
	break;
    case Input:
	out << "Input" << endl;
	break;
    case Select:
	out << "Select" << endl;
	break;
    case Slicer:
	out << "Slicer" << endl;
	break;
    case Lit:
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
	  ostream_iterator<gate_flag_t> (out, " "));
    out << endl;

    out << "comm: ";
    out << g.comment << endl << endl;

    out << "depth: ";
    out << g.depth << endl;

    return out;
    
}

