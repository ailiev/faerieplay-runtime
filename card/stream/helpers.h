// -*- c++ -*-

// some convenience things for the stream processor

#include <boost/array.hpp>
#include <boost/iterator/counting_iterator.hpp>
#include <boost/range.hpp>

#include <pir/common/utils.h>


/// identity item procedure for #stream_process, on batches of size 1 (without
/// using boost::array)
inline void identity_itemproc (size_t i, const ByteBuffer& in,
			       ByteBuffer & out)
{
    out = in;
}

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

	    out = ByteBuffer (&y, sizeof(y), ByteBuffer::DEEP);
	}

private:
    IntProc f;
};


// make a stream processing order range, with identical input and output
// indices, and counting from 0 to N
pir::transform_range<boost::iterator_range<boost::counting_iterator<index_t> >,
		     scalar2pair <index_t> >
zero_to_n (index_t N)
{
    return make_pair_range (pir::make_counting_range (0U, N));
}


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
pir::transform_range<Range,
		     scalar2pair <typename boost::range_value<Range>::type> >
make_pair_range (const Range& r)
{
    typedef typename boost::range_value<Range>::type val_t;

    return pir::make_transform_range (r,
				      scalar2pair<val_t>());
}


// template <size_t N>
// void identity_itemproc (const boost::array<size_t, N> &     i,
// 			const boost::array<ByteBuffer, N>&  in,
// 			boost::array<ByteBuffer, N> & 	    out)
// {
//     out = in;
// }

