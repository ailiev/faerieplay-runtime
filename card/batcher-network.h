// -*- c++ -*-
// LICENCE
// Jan 2004

#include <iostream>
#include <iomanip>
#include <algorithm>

#include <stdlib.h>


#include <pir/common/utils.h>
#include <pir/common/logging.h>
#include <pir/common/exceptions.h>

#include <pir/card/lib.h>


#ifndef _BATCHER_PERMUTE_H
#define _BATCHER_PERMUTE_H



namespace BatcherNetwork {
    extern Log::logger_t logger;

    DECL_STATIC_INIT(logger = Log::makeLogger ("batcher-network",
					       boost::none, boost::none));
}


DECL_STATIC_INIT_INSTANCE(BatcherNetwork);



// quick base class to pass information about the network location.
class BatcherInfoListener
{
public:

    // a new pass is starting.
    virtual void newpass() = 0;

    virtual ~BatcherInfoListener()
	{}
};



/// control loop for the batcher bitonic sorting network as described in CLRS.
/// @param _Comparator type should be callable as
/// compare (index_t a, index_t b)
/// to execute a comparator with these two wires.
/// @param N the number of elements to sort. Assume zero-based indexing.
///
/// the comparator could throw any exceptions, so this function can too.
template<typename _Comparator>
void run_batcher
(
    size_t N,
    _Comparator & compare,	// the comparator function
    BatcherInfoListener * listener = NULL
    )
    throw (better_exception)
{
    // the next power of two
    const size_t n = lgN_ceil (N);
    
    // merger number, from 1 to n
    for (unsigned m = 1; m <= n; m++) {
	
	unsigned M = 1U << m;	// M = 2^m, 2 -> N

	// do the inverted half-cleaners. g is the first top wire in
	// the group.
	LOG (Log::PROGRESS, BatcherNetwork::logger,
	     "Merger " << std::setw(3) << m << " half-cleaners @ "
	     << epoch_time);
	
	// can stop doing half-cleaner groups when the bottom rightmost wire in
	// the group > N-1, ie. go while that wire <= N-1, ie.
	//	g <= N - M/2 - 1,
	// or	g <  N - M/2
	for (unsigned g = 0; g < N - M/2; g += M)
	{
	    // the first bottom wire
	    const unsigned gb = g + (M-1);

	    // do the comparators in that group.
	    // start when bottom wire <= N-1,
	    // ie. gb - c   <= N-1
	    //     c	    >= gb - (N-1)
	    // make sure no screwups due to unsigned, as gb-N+1 may be negative
	    int start_c = std::max (0, int(gb) - (int(N)-1));
	    for (unsigned c = start_c; c < M/2; c++)
	    {
		compare (g + c, gb - c);
	    }
	}

	if (listener) listener->newpass();

	


	// do the remaining bitonic sorters in this merger, which look
	// like the last m-1 stages of an inverted butterfly network
	// on N inputs

	unsigned m_outer = m;

	// now we shadow the merger number m with the Cooley-Tukey m in here,
	// which is the size of a group of comparators.
	for (unsigned m = M/2; m >= 2; m >>= 1U)
	{
	    
	    LOG(Log::PROGRESS, BatcherNetwork::logger,
		"Merger " << std::setw(3) << m_outer
		<< ", bitonic sorter level "
		<< std::setw (3) << lgN_floor(m) << " @ "
		<< epoch_time);
	    
	    // g is the first wire in the current group
	    // can stop doing wire-groups when the bottom of the first wire is > N-1,
	    // ie.	g + m/2	> N - 1
	    //		g	> N-1 - m/2
	    // ie. go while
	    //		g	<= N-1 - m/2
	    // or	g	< N - m/2
 	    for (unsigned g = 0; g < N - m/2; g += m)
	    {
		
		LOG (Log::DEBUG, BatcherNetwork::logger,
		     "Group starting at g=" << g);
		
		// k is the comparator number in this group.
		// normally k < m/2 in this loop.
		// additionally, can stop doing comparators when the bottom wire
		// is > N-1,
		// ie.	g + k + m/2 > N-1
		//	k	    > N-1 - m/2 - g
		// ie. go while
		//	k	    <= N-1 - m/2 - g
		// or	k	    < N - m/2 - g
		unsigned k_max = std::min (m/2,  N - m/2 - g);
		for (unsigned k = 0; k < k_max; k++)
		{
		    compare (g + k, g + k + m/2);
		}
	    }

	    if (listener) listener->newpass();
	}


    }


}


#endif // _BATCHER_PERMUTE_H
