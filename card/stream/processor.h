// -*- c++ -*-

#include <boost/range.hpp>
#include <boost/array.hpp>
#include <boost/lambda/lambda.hpp>

#include <boost/mpl/size_t.hpp>
#include <boost/mpl/greater.hpp>
#include <boost/mpl/if.hpp>

#include <pir/common/utils.h>
#include <pir/common/range-utils.h>
#include <pir/common/logging.h>

#include <pir/card/io_flat.h>

#ifndef _STREAM_PROCESSOR_H
#define _STREAM_PROCESSOR_H


OPEN_NS




namespace StreamProcessor {
    extern Log::logger_t logger;

    DECL_STATIC_INIT (
	logger = Log::makeLogger ("circuit-vm.card.stream-processor");
	);
}


DECL_STATIC_INIT_INSTANCE(StreamProcessor);



// make an array of things, even if only one
template <class T, size_t N>
boost::array<T,N> &
make_array (boost::array<T,N> & in)
{
    return in;
}

template <class T>
boost::array<T,1>
make_array (const T& in)
{
    // NOTE: caved in and used double braces here, as the g++ warning produced
    // otherwise causes too much noise.
    boost::array<T,1> answer = { {in} };
    return answer;
}


template <size_t N>
size_t
sum_io_elem_size (const boost::array<FlatIO*, N>& ios)
{
    size_t z = 0;
    FOREACH (io, ios) {
	if (*io != 0)
	{
	    z += (*io)->getFilteredElemSize();
	}
    }
    return z;
}


template <class IdxRange>
void do_read (
    const IdxRange& idxs,
    std::vector<ByteBuffer> & o_objs,
    FlatIO * & io
//    , typename ensure_range_of<IdxRange, index_t>::type * tester1 = 0
    )
{
    ASSERT_RANGE_OF (IdxRange, index_t);
    
    // this must be the list version of FlatIO::read
    if (io) io->read (idxs, o_objs);
}

template <class IdxRange,
	  size_t I>
void do_read (
    const IdxRange& idxs, // a range (size C) of arrays (size I) of index_t
    std::vector<boost::array<ByteBuffer,I> > & o_objs,
    boost::array<FlatIO*, I> & ios
//     , typename ensure_range_of<IdxRange,
//                                boost::array<index_t,I> >::type * tester1 = 0
    )
{
    typedef std::vector<boost::array<ByteBuffer,I> > objs_t;

    typename boost::range_const_iterator<IdxRange>::type idxsi;
    typename boost::array<FlatIO*, I>::iterator iosi;
    unsigned slice;
    
    // iterate over the I/O streams, and the corresponding slices into the other
    // 2 containers.
    for (iosi = boost::begin (ios), slice = 0;
	 iosi != boost::end (ios);
	 ++iosi, ++slice)
    {
	if (*iosi)
	{
	    slice_range<typename objs_t::iterator> objs_slice (o_objs, slice);
	    (*iosi)->read (make_slice_range (idxs, slice),
			   objs_slice);
	}
    }
}




// overloaded versions of do_write, for the case where there is only one IO, and
// several IOs
template <class IdxRange,
	  size_t I>
void do_write (
    const IdxRange& idxs, // a range (size C) of arrays (size I) of index_t
    const std::vector<boost::array<ByteBuffer,I> > & objs,
    boost::array<FlatIO*, I> & ios
//     , typename ensure_range_of<IdxRange,
//                                boost::array<index_t,I> >::type * tester1 = 0
    )
{
    typedef boost::array<index_t,I> idx_arr_t;
    ASSERT_RANGE_OF (IdxRange, idx_arr_t);
    
    typename boost::range_const_iterator<IdxRange>::type idxsi;
    typename boost::array<FlatIO*, I>::const_iterator iosi;
    unsigned slice;
    
    // iterate over the I/O streams, and the corresponding slices into the other
    // 2 containers.
    for (iosi = boost::begin (ios), slice = 0;
	 iosi != boost::end (ios);
	 ++iosi, ++slice)
    {
	if (*iosi) (*iosi)->write (make_slice_range (idxs, slice),
				   make_slice_range (objs, slice));
    }
}


template <class IdxRange>
void do_write (
    const IdxRange& idxs,
    const std::vector<ByteBuffer> & objs,
    FlatIO * & io
//    , typename ensure_range_of<IdxRange, index_t>::type * tester1 = 0
    )
{
    ASSERT_RANGE_OF (IdxRange, index_t);
    
    // this must be the list version of FlatIO::write
    if (io) io->write (idxs, objs);
}



// my first metafunction!
/** define a type as an array if N>1
 * @param Scalar the basic type
 * @param N how many of the scalars
 * @return if N = 1, returns Scalar, otherwise boost::array<Scalar, N>
 */
template <class Scalar, size_t N>
struct make_maybe_array
{
    typedef typename boost::mpl::if_< typename boost::mpl::greater< boost::mpl::size_t<N>,
								    boost::mpl::size_t<1> >::type,
				      boost::array<Scalar, N>,
				      Scalar >::type
    type;
};




/**
   Process a container in stream mode.
   For all the template parameters, boost::array<X,N> can be replaced by just
   X if N=1
@param StreamOrder boost::ForwardRange, containing
       an iterator to std::pair<boost::array<index_t,N>>, where pair.first is
   the input index set, and pair.second is the output set.
@param StreamOrder same type as InStreamOrder
@param ItemProc ternary function object, with Arg1=boost::array<index_t>,
      Arg2=boost:array<ByteBuffer>, and Arg3=boost::array<ByteBuffer>. All the
      Arrays can be replaced by corresponding scalars if N=1
@param N The number of items to be read in, processed, and written out at a
       time.
@paramm in_order the sequence of indices to read in.
@paramm
*/



/**
   @param B the batch size - how many indices and objects in each operation.
   mostly one, sometimes 2.
   @param I the number of I/O streams feeding to and from each operation.
   @param StreamOrder The type of the order of (input and output) indices to be fed to the
   operation. 
   @param ItemProc the type of the operation function. The signature should be
   ItemProc (index_t, const ByteBuffer&, ByteBuffer& out)
   If B or I are > 1, the scalar type becomes a boost::array of the
   appropropriate length, 2D array if both I and B are > 1.
*/
template <class ItemProc,
	  class StreamOrder,
	  std::size_t B,
	  std::size_t I>
struct stream_processor {
    
    // the types for each IO stream
    typedef typename make_maybe_array<size_t,     B>::type  io_idx_batch_t;
    typedef typename make_maybe_array<ByteBuffer, B>::type  io_obj_batch_t;

    // and the overall types
    // each one is either X, array<X,(I or B)> or array<array<X,B>,I>, for X in
    // {size_t, ByteBuffer}
    typedef typename make_maybe_array<io_idx_batch_t, I>::type	idx_batch_t;
    typedef typename make_maybe_array<io_obj_batch_t, I>::type  obj_batch_t;

    // the type of the IO stream, either a single or an array of FlatIO*
    typedef typename make_maybe_array<FlatIO*, I>::type io_t;
    

    // the iterator type carried by StreamOrder
    typedef typename boost::range_const_iterator<StreamOrder>::type order_itr_t;

    typedef std::pair<idx_batch_t,idx_batch_t> idx_batch_pair;
    ASSERT_RANGE_OF (StreamOrder,
		     idx_batch_pair);


    // FIXME: the array code failed (with an XDR encoding error on the host)
    // when using 4K cache, and 4K-element array. Reducing cache to 1K works.
    static const size_t CACHESIZE = 1*(1<<10); // 1K bytes cache memory


    static void process (ItemProc & itemproc,
			 const StreamOrder & order,
			 io_t & in,
			 io_t & out)
	{
	    // how many elements to cache into an I/O operation?
	    // NOTE: sum_io_elem_size uses the filtered element size to account
	    // for a larger filtered (eg. encrypted) object size.
	    size_t C = CACHESIZE /
		std::max (sum_io_elem_size (make_array (in)),
			  sum_io_elem_size (make_array (out)));

	    // just in case...
	    if (C == 0) C = 1;
	    
	    const size_t N = boost::size (order);

	    LOG (Log::DUMP, StreamProcessor::logger,
		 "sum_io_elem_size (in) = " << sum_io_elem_size (make_array (in))
		 << "sum_io_elem_size (out) = " << sum_io_elem_size (make_array (out)) );
	    
	    LOG (Log::DEBUG, StreamProcessor::logger,
		 "process() has N = " << N << "; C = " << C);
	    
	    unsigned i;
	    size_t thisC;
	    order_itr_t idx_batch_i;
	    for (idx_batch_i =  boost::const_begin (order), i=0, thisC=std::min (N-i, C);
		 idx_batch_i != boost::const_end (order);
		 i+=thisC, idx_batch_i += thisC, thisC = std::min (N-i, C))
	    {
		
		std::vector<obj_batch_t>
		    cached_objs   (thisC),
		    cached_f_objs (thisC);

		// why oh why no type deduction??
		typedef transform_range <
		    boost::iterator_range<order_itr_t>,
		    get_first<idx_batch_t,idx_batch_t> >
		    in_idx_range_t;
		
		in_idx_range_t in_idx_range (
		    boost::make_iterator_range (idx_batch_i,
						idx_batch_i + thisC),
		    get_first<idx_batch_t,idx_batch_t>() );
		

		do_read (in_idx_range, cached_objs, in);
			 
	
		// could use an std::transform here instead of the loop, except for the return
		// parameter of itemproc
		typename boost::range_iterator<in_idx_range_t>::type idxi
		    (boost::begin (in_idx_range));
		typename std::vector<obj_batch_t>::const_iterator obji
		    (cached_objs.begin());
		typename std::vector<obj_batch_t>::iterator	  f_obji
		    (cached_f_objs.begin());
		for ( ;
		     obji != cached_objs.end();
		     ++idxi, ++obji, ++f_obji)
		{
		    assert (idxi != boost::end (in_idx_range));
		    assert (f_obji != cached_f_objs.end());
		    
		    itemproc (*idxi, *obji, *f_obji);
		}
	
		typedef transform_range <
		    boost::iterator_range<order_itr_t>,
		    get_second<idx_batch_t,idx_batch_t> >
		    out_idx_range_t;
		
		out_idx_range_t out_idx_range (
		    boost::make_iterator_range (idx_batch_i,
						idx_batch_i + thisC),
		    get_second<idx_batch_t,idx_batch_t>() );
		

		do_write (out_idx_range, cached_f_objs, out);


	    } // for (...)

	    LOG (Log::DEBUG, StreamProcessor::logger,
		 "process() done");
	} // function process(...)

};


//
// function front-ends
//

template <class ItemProc,
	  class StreamOrder,
	  class IO,
	  std::size_t B,
	  std::size_t I>
void stream_process (ItemProc & itemproc,
		     const StreamOrder & order,
		     IO& in,
		     IO& out,
		     boost::mpl::size_t<B>,
		     boost::mpl::size_t<I>)
{
    typedef stream_processor<ItemProc,StreamOrder,B,I> processor_class_t;

    processor_class_t::process (itemproc,
				 order,
				 in, out);
}

// and with const proc
template <class ItemProc,
	  class StreamOrder,
	  class IO,
	  std::size_t B,
	  std::size_t I>
void stream_process (const ItemProc & itemproc,
		     const StreamOrder & order,
		     IO& in,
		     IO& out,
		     boost::mpl::size_t<B>,
		     boost::mpl::size_t<I>)
{
    typedef stream_processor<const ItemProc,StreamOrder,B,I> processor_class_t;

    processor_class_t::process (itemproc,
				 order,
				 in, out);
}


/** overloaded versions for case where B=1 and I=1, for const and non-const proc
*/

template <class ItemProc,
	  class StreamOrder>
void stream_process (ItemProc & itemproc,
		     const StreamOrder & order,
		     FlatIO * in,
		     FlatIO * out)
{
    stream_process (itemproc, order, in, out,
		    boost::mpl::size_t<1>(),
		    boost::mpl::size_t<1>());
}

template <class ItemProc,
	  class StreamOrder>
void stream_process (const ItemProc & itemproc,
		     const StreamOrder & order,
		     FlatIO * in,
		     FlatIO * out)
{
    stream_process (itemproc, order, in, out,
		    boost::mpl::size_t<1>(),
		    boost::mpl::size_t<1>());
}


CLOSE_NS


#endif // _STREAM_PROCESSOR_H
