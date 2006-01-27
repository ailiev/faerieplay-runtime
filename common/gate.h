// -*- c++ -*-
#include <list>
#include <vector>
#include <string>
#include <ostream>

#include <pir/common/exceptions.h>

#ifndef _GATE_H
#define _GATE_H







struct gate_t {

    enum gate_op_kind_t {
	BinOp,
	UnOp,
	ReadDynArray,
	WriteDynArray,
	Input,
	Select,
	Slicer,
	Lit,
	InitDynArray
    };



    enum binop_t {
	Plus,
	Minus,
	
	Times,
	Div,
	Mod,

	Eq,				// equality
	LT,				// comparisons
	GT,
	LTEq,
	GTEq,
	NEq,

	SR,				// shifts
	SL
    };

    enum unop_t {
	Negate,
	LNot,
	BNot
    };

    enum gate_flag_t {
	Output
    };

        
    enum typ_kind_t {
	Array,			// params: <length> and <size of element type>
	Scalar
    };


    struct typ_t {
	typ_kind_t 	kind;
	int 	params [4];
    };

    struct gate_op_t {
	gate_op_kind_t  kind;
	int 	    params[4];	// currently need a max of 3 params (for
				// gate_t::WriteDynArray), but just plan ahead
    };


    

    gate_t();

    int 		    num;
    int			    depth;
    typ_t 		    typ;
    gate_op_t 		    op;
    std::vector<int>	    inputs; // the input gate numbers
    std::list<gate_flag_t>  flags;
    std::string		    comment;

};



int do_bin_op (gate_t::binop_t op, int x, int y);

int do_un_op (gate_t::unop_t op, int x);


gate_t unserialize_gate (const std::string& gate)
    throw (io_exception);


std::ostream& print_gate (std::ostream & out, const gate_t & g);


#endif // _GATE_H
