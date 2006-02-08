// -*- c++ -*-

#include <boost/range.hpp>
#include <boost/array.hpp>
#include <boost/lambda/core.hpp> // for _1 etc
#include <boost/lambda/lambda.hpp>

#include <boost/mpl/size_t.hpp>

#include <pir/common/utils.h>
#include <pir/common/range-utils.h>

#include <pir/card/io_flat.h>

#ifndef _STREAM_PROCESSOR_H
#define _STREAM_PROCESSOR_H

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

// degrees of freedom:
// 1 - batch size N
// 2 - itemproc is filter (in->out), generate (->out), or absorb (in->)
//       decides: - how many FlatIO's need to be provided (can be achieved through
//                  boost::optional or pointers.
//                - how itemproc shoud be called: with 2 or 3 params.
/// @param MkObjRange type of a function (object) which we will use to make a
/// range to send to FlatIO::write. If the obj_batch_t is a container, it itself
/// is a range, so we use identity<>. If obj_batch_t is a scalar ByteBuffer,
/// we'll use a scalar_range to wrap it.
template <class ItemProc,
	  class StreamOrder,
	  std::size_t N,
	  class IdxBatch=boost::array<size_t, N>,
	  class ObjBatch=boost::array<ByteBuffer, N>,
	  class MkOutObjsT=identity<ObjBatch>
>
struct stream_processor {
    
    typedef IdxBatch idx_batch_t;
    typedef ObjBatch obj_batch_t;

    
    static void process (ItemProc & itemproc,
			 const StreamOrder & order,
			 FlatIO * in,
			 FlatIO * out,
			 const MkOutObjsT & mk_out_objs = identity<obj_batch_t>())
	{
	    // read in the next needed items, apply the itemproc to them, and
	    // write them out again
	    typename boost::range_iterator<StreamOrder>::type idxsi; // an iterator
    
	    for (idxsi = boost::const_begin (order);
		 idxsi != boost::const_end (order);
		 idxsi++)
	    {
		std::pair<idx_batch_t, idx_batch_t> idxs = *idxsi;
	
		obj_batch_t objs, f_objs;
	
		if (in) {
		    // if objs is an array, mk_out_objs should be
		    // identity<array...>
		    // if objs is a scalar ByteBuffer, mk_out_objs should be
		    // addressof<ByteBuffer>
		    in->read (idxs.first,
			      mk_out_objs(objs));
		}
	
		itemproc (idxs.first, objs, f_objs);
	
		if (out) {
		    out->write (idxs.second, f_objs);
		}
	    }
	}
};


// specialize for case where N=1, dropping all the array and just using straight
// scalar.
template <class ItemProc,
	  class StreamOrder>
struct stream_processor<ItemProc,StreamOrder,1>
{
    typedef addressof<ByteBuffer> mk_out_obj_t;
    
    static void process (
	ItemProc & itemproc,
	const StreamOrder & order,
	FlatIO * in,
	FlatIO * out)
	{
	    stream_processor<
		ItemProc, StreamOrder, 1,
		size_t, ByteBuffer,
		mk_out_obj_t
		>
		::process
		(
		    itemproc, order, in, out, addressof<ByteBuffer>()
		    );
	}

// 	    typedef size_t	idx_batch_t;
// 	    typedef ByteBuffer  obj_batch_t;
    
// 	    // read in the next needed items, apply the itemproc to them, and
// 	    // write them out again
// 	    typename boost::range_iterator<StreamOrder>::type idxsi; // an iterator	    
    
// 	    for (idxsi = boost::const_begin(order);
// 		 idxsi != boost::const_end (order);
// 		 idxsi++)
// 	    {
// 		std::pair<idx_batch_t, idx_batch_t> idxs = *idxsi;
	
// 		obj_batch_t objs, f_objs;
	
// 		in.read (idxs.first,
// 			 objs);
	
// 		itemproc (idxs.first, objs, f_objs);
	
// 		out.write (idxs.second, f_objs);
// 	    }
// 	}
};

template <class ItemProc,
	  class StreamOrder,
	  std::size_t N>
void stream_process (
    ItemProc & itemproc,
    const StreamOrder & order,
    FlatIO * in,
    FlatIO * out,
    boost::mpl::size_t<N>)
{
    stream_processor<ItemProc,StreamOrder,N>::process
	(itemproc, order, in, out);
}


/** a convenience version where the input and output orders are the same
    @param N the batch size
    @param StreamOrder an iterator_range with iterator pointing to
    boost::array<size_t, N>
*/
template <class ItemProc,
	  class StreamOrder,
	  std::size_t N,
	  class IdxBatch=boost::array<size_t, N> // This can be specified by
						 // users, eg. to use a scalar
						 // instead of an array if N=1
>
struct stream_processor_itr
{
    static void process (ItemProc & itemproc,
		  const StreamOrder & order,
		  FlatIO * in,
		  FlatIO * out,
		  boost::mpl::size_t<N> n)
	{
	    stream_process (
		itemproc,
		pir::transform_range (
		    order,
		    std::ptr_fun (scalar2pair<IdxBatch>)),
		in, out,
		n);
	}
};


template <class ItemProc,
	  class StreamOrder,
	  std::size_t N>
void stream_process_itr (ItemProc & itemproc,
			 const StreamOrder & order,
			 FlatIO * in,
			 FlatIO * out,
			 boost::mpl::size_t<N> n)
{
    stream_processor_itr<ItemProc,StreamOrder,N>::process (itemproc, order, in, out, n);
}


/** overloaded version for case where N=1
    @param StreamOrder an iterator_range with iterator pointing to size_t
*/

template <class ItemProc,
	  class StreamOrder>
void stream_process_itr (ItemProc & itemproc,
			 const StreamOrder & order,
			 FlatIO * in,
			 FlatIO * out
    )
{
    using namespace boost::lambda;

    stream_processor_itr<ItemProc, StreamOrder, 1, size_t>::process (itemproc, order, in, out,
								     boost::mpl::size_t<1>());
}



#endif // _STREAM_PROCESSOR_H
