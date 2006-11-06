#include <string>
#include <algorithm>		// for swap(), max and min

#include <math.h>

#include <boost/shared_ptr.hpp>
#include <boost/optional/optional.hpp> 
#include <boost/none.hpp>
#include <boost/array.hpp>

#include <pir/common/sym_crypto.h>
#include <pir/common/utils.h>
#include <pir/card/lib.h>
#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/io_filter_encrypt.h>

#include <common/gate.h>

#include "stream/processor.h"
#include "stream/helpers.h"

#include "utils.h"
#include "array.h"
#include "batcher-permute.h"
#include "runtime-exceptions.h"


OPEN_NS

using std::string;
using std::vector;
using std::auto_ptr;
using std::pair;
using std::make_pair;

using boost::shared_ptr;
using boost::optional;
using boost::array;

using std::min;
using std::max;

using boost::optional;
using boost::none;


//
// options to use a watered down algorithm, if the full and proper one is
// failing.
//

// Do not do re-fetching within a retrieval session.
// FIXME: this does not work for now. It is not that useful anyway, as it takes
// more than O(1) time to provide parallel branches of the array.
// #define NO_REFETCHES

// no encrypt/mac on any of the containers
// #define NO_ENCRYPT

// Do not re-permute the main containers, just stick with one permutation.
// #define NO_REPERMUTE



// how to examine the working area indices files on the host:
// for f in *; do echo $f; od -t d4 -A n $f; done
// and the working area objects:
// for f in *; do echo $f; od -s -A n $f; done
// of course NO_ENCRYPT should be enabled for this to be useful.


// namespace {
//     Log::logger_t logger = Log::makeLogger ("array",
// 					    boost::none, boost::none);    
// }




namespace {
    // instantiate the static array map
    ArrayHandle::map_t ArrayHandle::_arrays;
    ArrayHandle::des_t ArrayHandle::_next_array_num = 1;
}


// the static logger
Log::logger_t Array::_logger;

INSTANTIATE_STATIC_INIT(Array);


/// @param size_params ->first is the array length, ->second is the element
/// size. boost::none if the array should already exist on the host.
Array::Array (const string& name,
	      const boost::optional <pair<size_t, size_t> >& size_params,
              CryptoProviderFactory * prov_fact)
    : _name		(name),
      
      N			(size_params ? size_params->first  : 0),
      _elem_size	(size_params ? size_params->second : 0),
      
      _prov_fact	(prov_fact),
      _rand_prov	(_prov_fact->getRandProvider()),

      _num_retrievals	(0)

{

    _A = auto_ptr<ArrayA> (new ArrayA (name,
				       size_params, &N, &_elem_size,
				       _prov_fact));

    // if the array exists, get its sizes, and fill in the size-dependent
    // params.
    if (!size_params) {
	N 	   = _A->_io.getLen();
	_elem_size = _A->_io.getElemSize();
    }

    _max_retrievals = lrint (sqrt(float(N))) *  lgN_floor(N);
    // this happens for very small N (<= 16)
    if (_max_retrievals >= N) {
	_max_retrievals = N-1;
    }

    // init to identity permutation, and then do repermute()
    _p 		    = shared_ptr<TwoWayPermutation> (new IdPerm (N));

    _T = auto_ptr<ArrayT> (
	new ArrayT (_name,
		    _max_retrievals, _elem_size,
		    &_num_retrievals,
		    _prov_fact));
}



/// write a value directly, without any permutation etc.
void Array::write_clear (index_t idx, size_t off, const ByteBuffer& val)
    throw (host_exception, comm_exception)
{
    ByteBuffer current;

    _A->_io.read (idx, current);
    
    assert (val.len() <= current.len() - off);
    
    // splice in the new value
    memcpy (current.data() + off, val.data(), val.len());

    _A->_io.write (idx, current);
}




ByteBuffer Array::read (index_t idx)
    throw (better_exception)
{
    // need to:
    // 
    // - add either idx or (rand_idx <- a random untouched index) to the touched
    //   list
    // - read all elements in the touched list, while keeping A[idx]
    // - return A[idx] to caller


    LOG (Log::DEBUG, _logger,
	 "Array::read idx=" << idx);

    index_t p_idx = _p->p(idx);

    ByteBuffer buf;
    
#ifdef NO_REFETCHES

    // read straight out of the array
    _A->_io.read (p_idx, buf);
    _num_retrievals++;

#else
    
    append_new_working_item (p_idx);

    buf = _T->do_dummy_accesses (p_idx,
				 boost::none);

#endif
    
    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }

    return buf;
}


void Array::write (bool enable,
		   index_t idx, size_t off,
		   const ByteBuffer& val)
    throw (better_exception)
{
    
    LOG (Log::DEBUG, _logger,
	 "Array::write idx=" << idx);
    
    index_t p_idx = _p->p(idx);

#ifdef NO_REFETCHES

    // write straight into the array
    _A->_io.write (p_idx, val);
    _num_retrievals++;

#else
    
    append_new_working_item (p_idx);
    
    // if we pass a 'none' optional here, the item will not be updated (but
    // re-encrypted as always)
    (void) _T->do_dummy_accesses (p_idx,
				  enable ?
				  Just    (std::make_pair (off, val)) :
				  Nothing (std::make_pair (off, val)));

#endif

    if (_num_retrievals > _max_retrievals) {
	repermute ();
    }
}



void Array::repermute ()
{
    LOG (Log::PROGRESS, _logger,
	 "Array::repermute() on array " << _name
	 << ", " << N  << " elems");
    

#ifndef NO_REFETCHES
    // copy values out of the working areas and into their main array
    merge (*_T, *_A);
#endif // NO_REFETCHES

    
#ifndef NO_REPERMUTE
    // the new permutation - a range-adapted LR perm.
    shared_ptr<TwoWayPermutation> p2_
	(new UnbalancedLRPermutation (lgN_ceil(N),
				      lgN_ceil(N),
				      _prov_fact));
    p2_->randomize();

    shared_ptr<TwoWayPermutation> p2 (new RangeAdapterPermutation (p2_, N));

    _A->repermute (_p, p2);

    _p = p2;
#endif

    
    _num_retrievals = 0;
}




/// add a new distinct element to T:  either idx or a random
/// index (not already in T).
///
/// working with physical indices here
void Array::append_new_working_item (index_t idx)
    throw (better_exception)
{
    index_t rand_idx;
    optional<index_t> to_append;
    
    // find an index to append to the working set.
    while (!to_append)
    {
	rand_idx = _rand_prov->randint (N);
	LOG (Log::DEBUG, _logger,
	     "While accessing physical idx " << idx
	     << ", trying rand_idx " << rand_idx);
	to_append = _T->find_fetch_idx (rand_idx, idx);
    }


    ByteBuffer obj;
    // append the corresponding item to T.
    _A->_io.read (*to_append, obj);
    _T->appendItem (*to_append, obj);
	
    _num_retrievals++;
}


/// merge a T into an A.
/// No access pattern difficulties, the adversary has already seen the indices
/// copied to the T.
void
Array::merge (const ArrayT& T, ArrayA& A)
{
    for (unsigned i=0; i < *T._num_retrievals; i++)
    {
	ByteBuffer item, idxbuf;
	T._idxs .read (i, idxbuf);
	T._items.read (i, item);

	A._io.write (bb2basic<index_t>(idxbuf), item);
    }
}






//
//
// class ArrayT
//
//

Array::ArrayT::ArrayT (const std::string& name, /// the array name
		       size_t max_retrievals,
		       size_t elem_size,
		       const size_t * p_num_retrievals,
		       CryptoProviderFactory * prov_fact)
    : _name		(name),
      _idxs		(_name + DIRSEP + "touched-idxs",
			 Just (std::make_pair (max_retrievals, sizeof(index_t)))),
      _items		(_name + DIRSEP + "touched-items",
			 Just (make_pair (max_retrievals, elem_size))),
      _num_retrievals	(p_num_retrievals),
      _prov_fact	(prov_fact)
{
#ifndef NO_ENCRYPT
    // add encrypt/decrypt filters to all the IO objects.
    FlatIO * ios [] = { &_idxs, &_items };
    for (unsigned i=0; i < ARRLEN(ios); i++) {
	ios[i]->appendFilter (
	    auto_ptr<HostIOFilter>
	    (new IOFilterEncrypt (ios[i],
				  shared_ptr<SymWrapper>
				  (new SymWrapper (_prov_fact)))));
    }
#endif
}





// result:
// have_target = do we have target_index in the container?
// have_rand   = do we have rand_idx in the container?
struct rand_idx_and_have_idx
{
    rand_idx_and_have_idx (index_t target_index,
			   index_t rand_idx_initval)
	: target_index	(target_index),
	  rand_idx	(rand_idx_initval),
	  have_target	(false),
	  have_rand	(false)
	{}
    
    void operator() (index_t, const ByteBuffer& in, ByteBuffer&)
	{
	    index_t T_i = bb2basic<index_t> (in);

	    if (T_i == target_index)
	    {
		have_target = true;
	    }
	    if (T_i == rand_idx)
	    {
		have_rand = true;
	    }
	}

    const index_t target_index;
    const index_t rand_idx;
    bool have_target;
    bool have_rand;		// is the proposed rand_idx in the working set
				// already?
};



boost::optional<index_t>
Array::ArrayT::
find_fetch_idx (index_t rand_idx, index_t target_idx)
{
    rand_idx_and_have_idx prog (target_idx, rand_idx);
    stream_process (prog,
		    zero_to_n (*_num_retrievals),
		    &_idxs,
		    NULL);

    if (!prog.have_target) {
	return Just(target_idx);
    }
    else if (!prog.have_rand) {
	return Just(rand_idx);
    }
    else {
	// target_idx and rand_idx both in there, try again.
	return none;
    }
}




class Array::ArrayT::dummy_fetches_stream_prog
{

    // named indexers into the above arrays
    enum {
	IDX=0,			// the item index
	ITEM=1			// the actual item
    };
	    
public:

    dummy_fetches_stream_prog (index_t target_idx,
			       const optional<pair<size_t,ByteBuffer> > &new_val,
			       size_t elem_size)
	: _target_idx (target_idx),
	  _new_val (new_val),
	  _elem_size (elem_size)
	{}
    
    void operator() (const boost::array<index_t,2>& idxs,
		     const boost::array<ByteBuffer,2>& objs,
		     boost::array<ByteBuffer,2>& o_objs)
	{
	    index_t 		T_i  = bb2basic<index_t> (objs[IDX]);
	    const ByteBuffer & 	item =			  objs[ITEM];

	    // do this in case the output goes to a different container
	    o_objs[IDX] = objs[IDX];
	    
	    if (T_i == _target_idx) {
		assert (((void)"dummy_fetches_stream_prog should not "
			 "see its target index more than once",
			 _the_item.len() == 0));

		_the_item = ByteBuffer (item, ByteBuffer::deepcopy());

		if (_new_val) {
		    // NOTE: if the new value is smaller than the current one,
		    // it will be spliced in at the specified offset, and the
		    // rest of the current data will remain.
		    
		    // splice in the new value at the given offset
		    ByteBuffer towrite = objs[ITEM];
		    bbcopy (towrite, _new_val->second, _new_val->first);

		    o_objs[ITEM] = towrite;
		}
		else {
		    // pass through
		    o_objs[ITEM] = objs[ITEM];
		}
	    }

	    else {
		// pass through, but will be written out re-encrypted with a new IV
		o_objs[ITEM] = objs[ITEM];
	    }
	    
	}

    ByteBuffer getTheItem ()
	{
	    assert (((void)"dummy_fetches_stream_prog should have "
		     "found the target item",
		     _the_item.len() > 0));

	    return _the_item;
	}
    
private:

    index_t _target_idx;
    const optional <pair<size_t, ByteBuffer> > & _new_val;

    size_t _elem_size;

    ByteBuffer _the_item;
};




ByteBuffer
Array::ArrayT::
do_dummy_accesses (index_t target_index,
		   const optional<pair<size_t, ByteBuffer> >& new_val)
{
    boost::array<FlatIO*,2> in_ios =  { &_idxs, &_items };
    boost::array<FlatIO*,2> out_ios = { &_idxs, new_val ? &_items : NULL };
    
    dummy_fetches_stream_prog prog (target_index, new_val,
				    _items.getElemSize());

    stream_process (prog,
		    zero_to_n<2> (*_num_retrievals),
		    in_ios,
		    out_ios,
		    boost::mpl::size_t<1> (), // number of items/idxs in an
					      // invocation of prog
		    boost::mpl::size_t<2> ()); // the number of I/O streams

    return prog.getTheItem ();
}

void
Array::ArrayT::
appendItem (index_t idx, const ByteBuffer& item)
{
    _idxs.write (*_num_retrievals, basic2bb (idx));
    _items.write (*_num_retrievals, item);
}





//
// class ArrayA
//



Array::ArrayA::ArrayA (const std::string& name, /// the array name
		       const optional<pair<size_t, size_t> >& size_params,
		       const size_t * N,
		       const size_t * elem_size,
		       CryptoProviderFactory *  prov_fact)
    : _name	    (name + DIRSEP + "array"),
      _io	    (_name, size_params),
      N		    (N),
      _elem_size    (elem_size),
      _prov_fact    (prov_fact)
{
#ifndef NO_ENCRYPT
    // add encrypt/decrypt filter.
    _io.appendFilter (
	auto_ptr<HostIOFilter>
	(new IOFilterEncrypt (&_io,
			      shared_ptr<SymWrapper>
			      (new SymWrapper (_prov_fact)))));
#endif

    // if array is new, fill out with nulls.
    if (size_params)
    {
	ByteBuffer zero (size_params->second); // the element size
	zero.set (0);
	stream_process (make_generate_proc_adapter (make_fconst(zero)),
			zero_to_n (size_params->first),	// array length
			NULL,
			&_io);
    }
}




void
Array::ArrayA::repermute (const shared_ptr<TwoWayPermutation>& old_p,
			  const shared_ptr<TwoWayPermutation>& new_p)
{
    // a container for the new permuted array.
    // its IOFilterEncrypt will setup the new keys
    shared_ptr<FlatIO> p2_cont_io (
	new FlatIO (_io.getName() + "-p2",
		    std::make_pair (_io.getLen(),
				    _io.getElemSize())));
#ifndef NO_ENCRYPT
    p2_cont_io->appendFilter (
	auto_ptr<HostIOFilter> (
	    new IOFilterEncrypt (
		p2_cont_io.get(),
		shared_ptr<SymWrapper> (new SymWrapper (_prov_fact)))));
#endif

    // copy values across
    stream_process ( identity_itemproc<>(),
		     zero_to_n (*N),
		     &_io,
		     &(*p2_cont_io) );
    
    // run the re-permutation on the new container
    shared_ptr<ForwardPermutation> reperm (new RePermutation (*old_p, *new_p));

    Shuffler shuffler  (p2_cont_io, reperm, *N);
    shuffler.shuffle ();

    // install the new parameters
    // NOTE: this invokes FlatIO::operator= which also moves stuff around on the
    // host.
    _io = *p2_cont_io;
}    






// ********************
/// \section s3456 class ArrayHandle
// ********************

ArrayHandle::des_t ArrayHandle::newArray (const std::string& name,
					  size_t len, size_t elem_size,
					  CryptoProviderFactory * crypt_fact)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name,
					 Just (std::make_pair (len, elem_size)),
					 crypt_fact));

    return insert_arr (arrptr);
}

ArrayHandle::des_t ArrayHandle::newArray (const std::string& name,
					  CryptoProviderFactory * crypt_fact)
    throw (better_exception)
{
    shared_ptr<Array> arrptr (new Array (name,
					 none,
					 crypt_fact));

    return insert_arr (arrptr);
}


ArrayHandle::des_t
ArrayHandle::insert_arr (const boost::shared_ptr<Array>& arr)
{
    des_t desc = _next_array_num++;

    // there should be no entry with idx
//    assert (_arrays.find(idx) == _arrays.end() );
    
    _arrays.insert (make_pair (desc,
			       ArrayHandle (arr, desc)));

    return desc;
}    


ArrayHandle &
ArrayHandle::getArray (ArrayHandle::des_t desc)
{
    map_t::iterator arr_i = _arrays.find (desc);
    if (arr_i == _arrays.end()) {
	throw illegal_operation_argument (
	    "ArrayHandle::getArray: non-existent array requested");
    }

//    return _arrays[desc];
    return arr_i->second;
}




ArrayHandle &
ArrayHandle::write (bool enable,
		    optional<index_t> idx, size_t off,
		    const ByteBuffer& val)
    throw (better_exception)
{
    LOG (Log::DEBUG, Array::_logger,
	 "Write on desc " << _desc);
    

    if (!idx) {
	LOG (Log::INFO, Array::_logger,
	     "ArrayHandle::write() got a null index");
	_arr->write (false, 0, off, val);
    }
    else if (*idx < 0 || *idx >= length()) {
	_arr->write (false, 0, off, val);
	LOG (Log::INFO, Array::_logger,
	     "ArrayHandle::write() got a outside-bounds index " << *idx);
    }
    else {
	// A Real Write!!
	_arr->write (enable, *idx, off, val);
    }
    
    return *this;
}


/// Read the value from an array index.
/// Encapsulates all the re-fetching and permuting, etc.
/// @param i the index to read
/// @param out the ByteBuffer with the value read out.
/// @return a (potentially new) ArrayHandle which should get subsequent reads to
/// that array branch. For now, the returned handle is always identical to the
/// passed-in one.
ArrayHandle &
ArrayHandle::read (bool enable,
		   optional<index_t> idx,
		   ByteBuffer & out)
    throw (better_exception)
{
    ByteBuffer dummy;
    
    LOG (Log::DEBUG, Array::_logger,
	 "Read on desc " << _desc);

    
    if (!enable) {
	dummy = _arr->read (0);
	out = getOptBBNil (_arr->elem_size());
    }	
    else if (!idx) {
	LOG (Log::INFO, Array::_logger,
	     "ArrayHandle::read() got a null index");
	
	dummy = _arr->read (0);
	out = getOptBBNil (_arr->elem_size());
    }
    else if (*idx < 0 || *idx >= length()) {
	LOG (Log::INFO, Array::_logger,
	     "ArrayHandle::read() got a outside-bounds index " << *idx);

	dummy = _arr->read (0);
	out = getOptBBNil (_arr->elem_size());
    }
    else{
	// The real read!!
	out = _arr->read (*idx);
	dummy = getOptBBNil (_arr->elem_size());
    }

    return *this;
}



#ifdef LOGVALS

// print an ArrayHandle's contents, in order from index 0 to N-1
std::ostream& operator<< (std::ostream& os, ArrayHandle& arr)
{
    ByteBuffer buf;

    os << "[";
    
    Array::print (os, *arr._arr);

    os << "]";

    return os;
}


// print an ArrayHandle's contents, in order from index 0 to N-1
std::ostream&
Array::print (std::ostream& os, Array& arr)
{
    // build a vector to indicate which elements are in T, and where in T they
    // are.
    // elem_in_T[i] =	-1	if actual element i is not in T,
    //			T_i	if actual element i is at T[T_i]
    std::vector<int> elem_in_T (arr.length(), -1);

    ArrayT& T = * arr._T;
    ArrayA& A = * arr._A;
    
    // go though T and build the lookup table elem_in_T.
    // _num_retrievals shows the number of active elements in T.
    for (unsigned i=0; i < *T._num_retrievals; i++)
    {
	index_t idx;
	ByteBuffer buf;
	
	T._idxs.read (i, buf);
	idx = bb2basic<index_t> (buf);
	assert (idx < elem_in_T.size());
	// elem_in_T should be indexed by actual index, while the value in
	// T._idxs is a permuted index, so apply the reverse permutation.
	elem_in_T[ arr._p->d(idx) ] = i;
    }

    // read all the items and print each.
    for (unsigned i=0; i < arr.length(); i++)
    {
	index_t idx;
	ByteBuffer buf;
	
	if (elem_in_T[i] == -1) {
	    // element is not in T, get it from A.
	    A._io.read (arr._p->p(i), buf);
	}
	else {
	    idx = elem_in_T[i];
	    T._items.read (idx, buf);
	}

	os << "{" << buf << "}";

	if (i+1 < arr.length()) {
	    os << ",";
	}
    }

    return os;
}
    
#endif // ifdef LOGVALS



CLOSE_NS
