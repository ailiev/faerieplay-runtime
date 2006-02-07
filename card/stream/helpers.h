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

// template <size_t N>
// void identity_itemproc (const boost::array<size_t, N> &     i,
// 			const boost::array<ByteBuffer, N>&  in,
// 			boost::array<ByteBuffer, N> & 	    out)
// {
//     out = in;
// }

