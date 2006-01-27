// -*- c++ -*-
// LICENCE
// Jan 2004


#ifndef _BATCHER_PERMUTE_H
#define _BATCHER_PERMUTE_H

#include <iostream>
#include <iomanip>

#include <stdlib.h>


#include <pir/common/utils.h>
#include <pir/common/exceptions.h>

#include <pir/card/lib.h>


// the _Comparator type should be callable as
// compare (index_t a, index_t b)
// to execute a comparator with these two wires
//
// the comparator could throw any exceptions, so this fcn can too
//
template<typename _Comparator>
void run_batcher
(
    size_t N,
    _Comparator & compare	// the comparator function
    )
    throw (better_exception)
{
    const size_t n = lgN_floor (N);
    
    // merger number, from 1 to n
    for (unsigned m = 1; m <= n; m++) {
	

	unsigned M = 1U << m;	// M = 2^m, 2 -> N

	// do the inverted half-cleaners. g is the first top wire in
	// the group
	std::clog << "Merger " << std::setw(3) << m << " half-cleaners @ "
	     << epoch_time << std::endl;
	
	for (unsigned g = 0; g < N; g += M) {
	    // the first bottom wire
	    unsigned gb = g + (M-1);

	    // do the comparators in that group
	    for (unsigned c = 0; c < M/2; c++) {
		compare (g + c, gb - c);
	    }
	}


	// do the remaining bitonic sorters in this merger, which look
	// like the last m-1 stages of an inverted butterfly network
	// on N inputs

	unsigned m_outer = m;
	// now we shadow m with the C-T m in here
	for (unsigned m = M/2; m >= 2; m >>= 1) {
	    
	    std::clog << "Merger " << std::setw(3) << m_outer
		      << ", bitonic sorter level "
		      << std::setw (3) << lgN_floor(m) << " @ "
		      << epoch_time << std::endl;
	    
	    // g is the first wire in the current group
	    for (unsigned g = 0; g < N; g += m) {
		
		// k is the comparator number in this group
		for (unsigned k = 0; k < m/2; k++) {
		    compare (g + k, g + k + m/2);
		}
	    }
	}


    }


}


#endif // _BATCHER_PERMUTE_H