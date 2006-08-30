// -*- c++ -*-
#include <list>
#include <vector>
#include <string>
#include <ostream>

#include <string.h>

#include <pir/common/exceptions.h>
#include <pir/common/utils.h>

#include <boost/optional/optional.hpp>


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
	SL,

	And,
	Or,

	BAnd,			// binary operations
	BOr,
	BXor
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
	Array,			// params:
				// <length> and
				// <size of element type, in bytes>
	Scalar
    };


    struct typ_t {
	typ_kind_t 	kind;
	int 	params [4];
    };

    struct gate_op_t {
	gate_op_kind_t		kind;
	int 	params[4]; // currently need a max of 3 params
	                                   // (for gate_t::WriteDynArray), but
	                                   // just plan ahead
    };


    

    gate_t();

    index_t 		    num;
    int			    depth;
    typ_t 		    typ;
    gate_op_t 		    op;
    std::vector<int>	    inputs; // the input gate numbers
    std::list<gate_flag_t>  flags;
    std::string		    comment;

};



boost::optional<int>
do_bin_op (gate_t::binop_t op,
	   boost::optional<int> x, boost::optional<int> y);


boost::optional<int> do_un_op (gate_t::unop_t op,
			       boost::optional<int> x);


gate_t unserialize_gate (const std::string& gate)
    throw (io_exception);


std::ostream& operator<< (std::ostream & out, const gate_t & g);



//
// HACK: miniamlistic serialization for optional<T> into a ByteBuffer
//
// uses the first byte to indicate whether the value is Just x or Nothing
//

template<class T>
boost::optional<T>
bb2optBasic (const ByteBuffer& buf)
{
    if (buf.data()[0] == 0)
    {
	return boost::optional<T> ();
    }
    else
    {
	// an alias
	ByteBuffer val (buf, 1, buf.len()-1);
	return bb2basic<T> (val);
    }
}

template<class T>
ByteBuffer
optBasic2bb (const boost::optional<T>& x)
{
    ByteBuffer answer (1 + sizeof(T));

    byte isJust = bool(x);

    memcpy (answer.data(), &isJust, 1);

    if (x)
    {
	memcpy (answer.data() + 1, x.get_ptr(), sizeof(*x));
    }

    return answer;
}

// is a byte buffer holding an encoded optional values (as done by optBasic2bb)
// initialized (Just)?
inline
bool isOptBBJust (const ByteBuffer& buf)
{
    return buf.data()[0] != 0;
}

// set a ByteBuffer (of any size >= 1) to NIL, by setting the first byte to 0
inline
void makeOptBBNothing (ByteBuffer& io_buf)
{
    io_buf.data()[0] = 0;
}

// make an Optional ByteBuffer contain the specified bytes.
inline
void makeOptBBJust (ByteBuffer & io_buf, const void * bytes, size_t len)
{
    assert (io_buf.len() >= len+1);
    
    io_buf.data()[0] = 1;
    memcpy (io_buf.data() + 1, bytes, len);
}


#endif // _GATE_H
