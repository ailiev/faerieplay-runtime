enum gate_op_type_x {
    Bin,
    Un,
    ReadDynArray,
    WriteDynArray,
    Input,
    Select,
    Slicer,
    Lit
};


enum bin_op_x {
    Times,
    Div ,
    Mod,
    Plus ,
    Minus ,
    SL ,
    SR ,
    Lt ,
    Gt ,
    LtEq ,
    GtEq ,
    Eq ,
    Neq ,
    BAnd ,
    BXor ,
    BOr ,
    And ,
    Or
};

enum un_op_x {
    Not,
    BNot,
    Neg,
    Log2,
    Bitsize
};

enum lit_type_x {
    LInt,
    LBool
};

union lit_x switch (lit_type_x lit_type) {
 case LInt:
     int i;
 case LBool:
     bool b;
};

struct slice_x {
    int off;
    int len;
};


union gate_op_x switch (gate_op_type_x gate_type_op) {
 case Bin:
     bin_op_x	op_bin_op;
 case Un:
     un_op_x	op_un_op;
 case ReadDynArray:
     void;
 case WriteDynArray:
     slice_x slice;
 case Input:
     void;
 case Select:
     void;
 case Slicer:
     slice_x slice;
 case Lit:
     lit_x lit;
};
