// -*- c++ -*-
/*
Copyright (C) 2003-2007, Alexander Iliev <sasho@cs.dartmouth.edu>

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.
* Neither the name of the author nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.

This product includes cryptographic software written by Eric Young
(eay@cryptsoft.com)

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

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

/// For a Just ByteBuffer, return its value.
inline ByteBuffer
getOptBBJustVal (const ByteBuffer& buf)
{
    assert (isOptBBJust (buf));

    // return an alias ByteBuffer
    return ByteBuffer (buf, 1, buf.len() - 1);
}


// set a ByteBuffer (of any size >= 1) to NIL, by setting the first byte to 0
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
