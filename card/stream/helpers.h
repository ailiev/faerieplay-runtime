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

/// function object adapter to adapt a function (object) working on ints, to a
/// standard one working on ByteBuffer.
/// @param IntProc a function f of type (idx, x) -> y
template <class IntProc>
struct int_itemproc {
    int_itemproc (const IntProc& f)
	: f(f)
	{}
    
    void operator () (size_t i, const ByteBuffer& in,
		      ByteBuffer & out)
	{
	    int x, y;
	    memcpy (&x, in.data(), in.len());
	    y = f (i, x);
	    out = ByteBuffer (&y, sizeof(y), ByteBuffer::DEEP);
	}

private:
    IntProc f;
};



// template <size_t N>
// void identity_itemproc (const boost::array<size_t, N> &     i,
// 			const boost::array<ByteBuffer, N>&  in,
// 			boost::array<ByteBuffer, N> & 	    out)
// {
//     out = in;
// }

