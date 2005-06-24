// -*- c++ -*-
#include <list>
#include <vector>
#include <string>
#include <ostream>

#include <pir/common/exceptions.h>


enum gate_op_kind_t {
    BinOp,
    UnOp,
    ReadDynArray,
    WriteDynArray,
    Input,
    Select,
    Slicer,
    Lit
};

struct gate_op_t {
    gate_op_kind_t kind;
    int param1, param2;
};



enum binop_t {
    Plus,
    Minus,
    Times,
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
    Array,
    Scalar
};


struct typ_t {
    typ_kind_t kind;
    int param;
};



struct gate_t {
    int 		num;
    typ_t 		typ;
    gate_op_t 		op;
    std::vector<int>		inputs;
    std::list<gate_flag_t> 	flags;
    std::string		comment;
};


int do_bin_op (binop_t op, int x, int y);

int do_un_op (unop_t op, int x);


gate_t unserialize_gate (const std::string& gate)
    throw (io_exception);


std::ostream& print_gate (std::ostream & out, const gate_t & g);

