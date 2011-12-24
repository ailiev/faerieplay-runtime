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

// some convenience things for the stream processor

#include <boost/array.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/range.hpp>

#include <faerieplay/common/utils.h>


#ifndef _STREAM_HELPERS_H
#define _STREAM_HELPERS_H


OPEN_NS

/// identity item procedure for #stream_process, on arbitrary size batches
template <size_t N=1U>
struct identity_itemproc
{
    void operator() (const boost::array<size_t,N> &,
		     const boost::array<ByteBuffer,N> & in,
		     boost::array<ByteBuffer,N> & out) const
	{
	    out = in;
	}
};

/// identity item procedure for #stream_process, on batches of size 1 (without
/// using boost::array)
template<> struct identity_itemproc<1U>
{
    void operator() (size_t i, const ByteBuffer& in,
		     ByteBuffer & out) const
	{
	    out = in;
	}
};




/// function object adapter to adapt an adaptable binary function working on a
/// basic C++ type, to a standard one working on ByteBuffer.
/// 
/// @param IntProc an Adaptable Binary Function with Arg1=index_t, Arg2=Result=a
/// basic C type
template <class IntProc>
struct basictype_itemproc {

    typedef typename IntProc::Arg2 T;
    
    basictype_itemproc (const IntProc& f)
	: f(f)
	{}
    
    void operator () (size_t i, const ByteBuffer& in,
		      ByteBuffer & out)
	{
	    T x, y;
	    
	    assert (sizeof(x) == in.len());
	    memcpy (&x, in.data(), in.len());
	    
	    y = f (i, x);

	    out = ByteBuffer (&y, sizeof(y), ByteBuffer::deepcopy());
	}

private:
    IntProc f;
};


#if 0
// adapt an itemproc working on some streamable C++ type to a standard one
template <class Proc>
struct streamtype_itemproc
{
    typedef typedef Proc::second_argument_type T;

    streamtype_itemproc (const Proc& f)
	: f(f)
	{}

    void operator() (size_t i, const ByteBuffer& in,
		     ByteBuffer & out)
	{
	    T x, y;

	    std::string str;
	    std::istringstream str
	    
	    y = f(i, x

		  }
	}

};
#endif // 0


template <class BasicProc>
basictype_itemproc<BasicProc>
make_basictype_itemproc (const BasicProc& f)
{
    return basictype_itemproc<BasicProc> (f);
}


//
// adapt item process functions to GENERATE and AGGREGATE processing
//

// function of the index only - unary
template <class ItemProc>
struct unary_proc_adapter {
    unary_proc_adapter (const ItemProc& proc)
	: f(proc)
	{}
    
    void operator() (index_t i, const ByteBuffer& in, ByteBuffer & out)
	{
	    out = f (i);
	}
    
    ItemProc f;
};

template <class ItemProc>
unary_proc_adapter<ItemProc>
make_unary_proc_adapter (const ItemProc& proc)
{
    return unary_proc_adapter<ItemProc> (proc);
}


// GENERATE without parameters
template <class ItemProc>
struct generate_proc_adapter {
    generate_proc_adapter (const ItemProc& proc)
	: f(proc)
	{}
    
    void operator() (index_t i, const ByteBuffer& in, ByteBuffer & out) const
	{
	    out = f ();
	}
    
    void operator() (index_t i, const ByteBuffer& in, ByteBuffer & out)
	{
	    out = f ();
	}

    ItemProc f;
};

template <class ItemProc>
generate_proc_adapter<ItemProc>
make_generate_proc_adapter (const ItemProc& proc)
{
    return generate_proc_adapter<ItemProc> (proc);
}




// AGGREGATE
template <class ItemProc>
struct aggregate_proc_adapter {
    aggregate_proc_adapter (const ItemProc& proc)
	: f(proc)
	{}
    
    void operator() (index_t i, const ByteBuffer& in, ByteBuffer & out)
	{
	    f (i, in);
	}
    
    ItemProc f;
};

template <class ItemProc>
aggregate_proc_adapter<ItemProc>
make_aggregate_proc_adapter (const ItemProc& proc)
{
    return aggregate_proc_adapter<ItemProc> (proc);
}




template <class Range>
#define PARENTTYPE \
boost::iterator_range<boost::transform_iterator<scalar2pair<typename boost::range_value<Range>::type>, \
                                                typename boost::range_iterator<Range>::type> >
struct pair_range : public PARENTTYPE
{
    typedef PARENTTYPE parent_t;
    typedef typename boost::range_value<Range>::type val_t;
    
    pair_range (const Range& r)
	: parent_t (pir::make_transform_range (r,
					       scalar2pair<val_t>()))
	{}
};
#undef PARENTTYPE


template <class Range>    
pair_range<Range>
make_pair_range (const Range& r)
{
    return pair_range<Range> (r);
}


template <class Range, size_t N>
#define PARENTTYPE \
boost::iterator_range<boost::transform_iterator<scalar2array<typename boost::range_value<Range>::type, N>, \
                                                typename boost::range_iterator<Range>::type> >
struct array_range : PARENTTYPE
{
    typedef PARENTTYPE parent_t;
    typedef typename boost::range_value<Range>::type val_t;
   
    array_range (const Range& r)
	: parent_t (pir::make_transform_range (r,
					       scalar2array<val_t, N> ()))
	{}
};
#undef PARENTTYPE

template <class Range, size_t N>
array_range<Range,N>
make_array_range (const Range& r,
		  boost::mpl::size_t<N>)
{
    return array_range<Range,N> (r);
}


/// make a stream processing order range, with identical input and output
/// indices, and counting from 0 to N
inline
pair_range<pir::counting_range<index_t> >
zero_to_n (index_t N)
{
    return make_pair_range (pir::make_counting_range (0U, N));
}

// another step up: this range iterates a pair of arrays, with the scalar
// duplicate into all that.
template <size_t I>
pair_range<array_range<pir::counting_range<index_t>,I> >
zero_to_n (index_t N)
{
    return make_pair_range (make_array_range (pir::make_counting_range (0U, N),
					      boost::mpl::size_t<I>()));
}


// template <size_t N>
// void identity_itemproc (const boost::array<size_t, N> &     i,
// 			const boost::array<ByteBuffer, N>&  in,
// 			boost::array<ByteBuffer, N> & 	    out)
// {
//     out = in;
// }


CLOSE_NS


#endif // _STREAM_HELPERS_H
