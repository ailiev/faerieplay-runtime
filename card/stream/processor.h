// -*- c++ -*-

#include <boost/range.hpp>
#include <boost/array.hpp>
#include <boost/lambda/core.hpp> // for _1 etc
#include <boost/lambda/lambda.hpp>

#include <boost/mpl/size_t.hpp>
#include <boost/mpl/greater.hpp>
#include <boost/mpl/if.hpp>

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
    using boost::mpl::if_;
    using boost::mpl::greater;
    using boost::mpl::size_t;
    
    typedef typename if_< typename greater< size_t<N>,
					    size_t<1> >::type,
			  boost::array<Scalar, N>,
			  Scalar >			    ::type	type;
};


template <class ItemProc,
	  class StreamOrder,
	  std::size_t B>	// the batch size
struct stream_processor {
    
    typedef typename make_maybe_array<size_t, B>::type	    idx_batch_t;
    typedef typename make_maybe_array<ByteBuffer, B>::type  obj_batch_t;
    
    typedef typename boost::range_iterator<StreamOrder>::type order_itr_t;
    
    static void process (ItemProc & itemproc,
			 const StreamOrder & order,
			 FlatIO * in,
			 FlatIO * out)
	{
	    // read in the next needed items, apply the itemproc to them, and
	    // write them out again

	    order_itr_t idxsi;
    
	    for (idxsi = boost::const_begin (order);
		 idxsi != boost::const_end (order);
		 idxsi++)
	    {
		std::pair<idx_batch_t, idx_batch_t> idxs = *idxsi;
	
		obj_batch_t objs, f_objs;
	
		if (in) {
		    // if objs is an array, mk_out_objs should be
		    // boost::begin<array...>
		    // if objs is a scalar ByteBuffer, mk_out_objs should be
		    // addressof<ByteBuffer>
		    in->read (idxs.first, objs);
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
// template <class ItemProc,
// 	  class StreamOrder>
// struct stream_processor<ItemProc,StreamOrder,1>
// {
//     static void process (
// 	ItemProc & itemproc,
// 	const StreamOrder & order,
// 	FlatIO * in,
// 	FlatIO * out)
// 	{
// 	    stream_processor<ItemProc, StreamOrder,  1,  size_t,ByteBuffer>
// 		::process (itemproc, order, in, out);
// 	}
// };




// so, can have a range-transform function, which takes the specialized order
// range for this particular invocation, and transforms it into the general
// pair<index_t> that stream_processor wants.

// versions of the processor function which has input and output indices identical.
template <class ItemProc,
	  class StreamOrder,
	  std::size_t N>
void stream_process (ItemProc & itemproc,
		     const StreamOrder & order,
		     FlatIO * in,
		     FlatIO * out,
		     boost::mpl::size_t<N> n)
{
    typedef stream_processor<ItemProc,StreamOrder,N> processor_class_t;

    processor_class_t::process (itemproc,
				 order,
				 in, out);
}


/** overloaded version for case where N=1
*/

template <class ItemProc,
	  class StreamOrder>
void stream_process (ItemProc & itemproc,
		     const StreamOrder & order,
		     FlatIO * in,
		     FlatIO * out)
{
    stream_process (itemproc, order, in, out,
		    boost::mpl::size_t<1>());
}





#endif // _STREAM_PROCESSOR_H
