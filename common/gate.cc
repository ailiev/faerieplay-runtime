#include <list>
#include <vector>
#include <string>
#include <ostream>
#include <sstream>
#include <iterator>

#include <pir/common/exceptions.h>

#include "gate.h"


using namespace std;


int do_bin_op (binop_t op, int x, int y) {

    switch (op) {
    case     Plus:  return x + y;
    case     Minus: return x - y;
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
	    line_str >> answer.typ.param;
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
		answer.op.param1 = Plus;
	    }
	    else if (word == "*") {
		answer.op.param1 = Times;
	    }
	    else if (word == "-") {
		answer.op.param1 = Minus;
	    }
	    else if (word == "<") {
		answer.op.param1 = LT;
	    }
	    else if (word == ">") {
		answer.op.param1 = GT;
	    }
	    else if (word == "==") {
		answer.op.param1 = Eq;
	    }
	    else if (word == ">=") {
		answer.op.param1 = GTEq;
	    }
	    else if (word == "<=") {
		answer.op.param1 = LTEq;
	    }
	    else if (word == "!=") {
		answer.op.param1 = NEq;
	    }
	    else if (word == "<<") {
		answer.op.param1 = SL;
	    }
	    else if (word == ">>") {
		answer.op.param1 = SR;
	    }
	    else {
		cerr << "unknown BinOp " << word << endl;
	    }
	}
	else if (word == "UnOp") {
	    answer.op.kind = UnOp;
	    line_str >> word;
	    if (word == "-") {
		answer.op.param1 = Negate;
	    }
	    else if (word == "!") {
		answer.op.param1 = LNot;
	    }
	    else if (word == "~") {
		answer.op.param1 = BNot;
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
	    line_str >> answer.op.param1;
	}
	else if (word == "ReadDynArray") {
	    answer.op.kind = ReadDynArray;
	}
	else if (word == "WriteDynArray") {
	    answer.op.kind = WriteDynArray;
	    line_str >> answer.op.param1;
	    line_str >> answer.op.param2;
	}
	else if (word == "Slicer") {
	    answer.op.kind = Slicer;
	    line_str >> answer.op.param1;
	    line_str >> answer.op.param2;
	
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
    // comment
    //
    getline (lines, answer.comment);

    return answer;
}


ostream& print_gate (ostream & out, const gate_t & g) {
    out << "num: " << g.num << endl;

    switch (g.typ.kind) {
    case Array:
	out << "Array " << g.typ.param << endl;
	break;
    case Scalar:
	out << "Scalar" << endl;
	break;
    }

    switch (g.op.kind) {
    case BinOp:
	out << "BinOp ";
	switch (g.op.param1) {
	case Plus:
	    out << "+" << endl;
	    break;
	case Minus:
	    out << "-" << endl;
	    break;
	case Times:
	    out << "*" << endl;
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
	switch (g.op.param1) {
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
	    out << "unknown " << g.op.param1 << endl;
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

    return out;
    
}

