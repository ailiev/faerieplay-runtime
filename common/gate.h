// -*- c++ -*-
/*
 * Circuit virtual machine for the Faerieplay hardware-assisted secure
 * computation project at Dartmouth College.
 *
 * Copyright (C) 2003-2007, Alexander Iliev <alex.iliev@gmail.com> and
 * Sean W. Smith <sws@cs.dartmouth.edu>
 *
 * All rights reserved.
 *
 * This code is released under a BSD license.
 * Please see LICENSE.txt for the full license and disclaimers.
 *
 */

#include <list>
#include <vector>
#include <string>
#include <ostream>

#include <string.h>

#include <faerieplay/common/exceptions.h>
#include <faerieplay/common/utils.h>

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
	Print,
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


    // The return type of this gate, with any needed parameters, eg. array size.
    struct typ_t {
	typ_kind_t 	kind;
	int 	params [4];
    };


    // The operation of this gate, with any needed (static) op parameters.
    // (the inputs are handled separately, via the 'inputs' field)
    struct gate_op_t {
	gate_op_kind_t		kind;
	int 	params[4]; // currently need a max of 3 params
	                                   // (for gate_t::WriteDynArray), but
	                                   // just plan ahead
    };


    

    gate_t();

    index_t 		    num;
    // TODO: get rid of the depth field, not needed when array gates have an
    // enable bit.
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
// uses the first byte to indicate whether the value is Just x or Nothing:
// 0: Nothing, the remaining bytes don't matter.
// 1: Just x, x serialized in the remaining bytes.
//

/// how many bytes for a flat version of an optional <type>?
#define OPT_BB_SIZE(type) (sizeof(type) + 1)

template<class T>
boost::optional<T>
bb2optBasic (const ByteBuffer& buf)
{
    assert (buf.len() == OPT_BB_SIZE(T));

    // TODO: explicitly implement the format spec where only the low-order bit
    // of the tag byte indicates if nil or not.
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

    // TODO: explicitly implement the format spec where the low-order bit of the
    // tag byte indicates if nil or not.
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
// FIXME: check just the first bit.
inline
bool isOptBBJust (const ByteBuffer& buf)
{
    return buf.data()[0] != 0;
}

/// For a Just ByteBuffer, return its value.
inline ByteBuffer
getOptBBJustVal (const ByteBuffer& buf)
{
    assert (isOptBBJust (buf));

    // return an alias ByteBuffer
    return ByteBuffer (buf, 1, buf.len() - 1);
}


// set a ByteBuffer (of any size >= 1) to NIL, by setting the first byte to 0
// FIXME: should set just the first bit.
inline
void makeOptBBNothing (ByteBuffer& io_buf)
{
    io_buf.data()[0] = 0;
}

/// make a NIL ByteBuffer of specified length
inline
ByteBuffer getOptBBNil (size_t len)
{
    ByteBuffer answer (len+1);
    makeOptBBNothing (answer);
    return answer;
}


/// make an Optional ByteBuffer contain the specified bytes.
inline
void makeOptBBJust (ByteBuffer & io_buf, const void * bytes, size_t len)
{
    assert (io_buf.len() >= len+1);
    
    io_buf.data()[0] = 1;
    memcpy (io_buf.data() + 1, bytes, len);
}


#endif // _GATE_H
