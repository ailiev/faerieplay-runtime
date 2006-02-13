// -*- c++ -*-

#include <boost/range.hpp>
#include <boost/array.hpp>
#include <boost/lambda/lambda.hpp>

#include <boost/mpl/size_t.hpp>
#include <boost/mpl/greater.hpp>
#include <boost/mpl/if.hpp>

#include <pir/common/utils.h>
#include <pir/common/range-utils.h>

#include <pir/card/io_flat.h>

#ifndef _STREAM_PROCESSOR_H
#define _STREAM_PROCESSOR_H




template <class IdxsType,
	  class ObjsType>
void do_read (const IdxsType& idxs, ObjsType& o_objs, FlatIO * & io)
{
    // this will resolve to either the scalar or list version of FlatIO::read
    if (io) io->read (idxs, o_objs);
}

template <class IdxsType,
	  class ObjsType,
	  size_t I>
void do_read (const IdxsType& idxs, ObjsType& o_objs, boost::array<FlatIO*, I> & ios)
{
    typename IdxsType::const_iterator idxsi;
    typename ObjsType::iterator objsi;
    typename boost::array<FlatIO*, I>::iterator iosi;
    
    for (idxsi = boost::begin(idxs),
	     objsi = boost::begin(o_objs),
	     iosi = boost::begin (ios);
	 idxsi != boost::end (idxs);
	 idxsi++, objsi++, iosi++)
    {
	if (*iosi) (*iosi)->read (*idxsi, *objsi);
    }
}

template <class IdxsType,
	  class ObjsType>
void do_write (const IdxsType& idxs, const ObjsType& objs, FlatIO * & io)
{
    if (io) io->write (idxs, objs);
}

template <class IdxsType,
	  class ObjsType,
	  size_t I>
void do_write (const IdxsType& idxs, const ObjsType& objs, boost::array<FlatIO*, I> & ios)
{
    typename IdxsType::const_iterator idxsi;
    typename ObjsType::const_iterator objsi;
    typename boost::array<FlatIO*, I>::iterator iosi;
    
    for (idxsi = boost::begin(idxs),
	     objsi = boost::begin(objs),
	     iosi = boost::begin (ios);
	 idxsi != boost::end (idxs);
	 idxsi++, objsi++, iosi++)
    {
	if (*iosi) (*iosi)->write (*idxsi, *objsi);
    }
}



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


// struct item_proc_call_t {
//     struct ITEMCALL_FULL {};
//     struct ITEMCALL_GENERATE {};
//     struct ITEMCALL_ABSORB {};
// };

// degrees of freedom:
// 1 - batch size N
// 2 - itemproc is filter (in->out), generate (->out), or absorb (in->)
//       decides: - how many FlatIO's need to be provided (can be achieved through
//                  boost::optional or pointers.
//                - how itemproc shoud be called: with 2 or 3 params.
/// @param MkOutObjsT type of a function (object) which we will use to make a
/// range to send to FlatIO::write. If the obj_batch_t is a container, it itself
/// is a range, so we use identity<>. If obj_batch_t is a scalar ByteBuffer,
/// we'll use a scalar_range to wrap it.


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


template <class ItemProc,
	  class StreamOrder,
	  std::size_t B,	// the batch size
	  std::size_t I>	// the number of I/O streams
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
    
    
    typedef typename boost::range_iterator<StreamOrder>::type order_itr_t;
    
    static void process (ItemProc & itemproc,
			 const StreamOrder & order,
			 io_t & in,
			 io_t & out)
	{
	    order_itr_t idxsi;
    
	    for (idxsi = boost::const_begin (order);
		 idxsi != boost::const_end (order);
		 idxsi++)
	    {
		std::pair<idx_batch_t, idx_batch_t> idxs = *idxsi;
	
		obj_batch_t objs, f_objs;

		do_read (idxs.first, objs, in);
	
		itemproc (idxs.first, objs, f_objs);
	
		do_write (idxs.second, f_objs, out);
	    }
	}

    static void process (const ItemProc & itemproc,
			 const StreamOrder & order,
			 io_t & in,
			 io_t & out)
	{
	    order_itr_t idxsi;
    
	    for (idxsi = boost::const_begin (order);
		 idxsi != boost::const_end (order);
		 idxsi++)
	    {
		std::pair<idx_batch_t, idx_batch_t> idxs = *idxsi;
	
		obj_batch_t objs, f_objs;

		do_read (idxs.first, objs, in);
	
		itemproc (idxs.first, objs, f_objs);
	
		do_write (idxs.second, f_objs, out);
	    }
	}    
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
    typedef stream_processor<ItemProc,StreamOrder,B,I> processor_class_t;

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





#endif // _STREAM_PROCESSOR_H
