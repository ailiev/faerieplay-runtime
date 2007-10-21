/*
 * Circuit virtual machine for the Faerieplay hardware-assisted secure
 * computation project at Dartmouth College.
 *
 * Copyright (C) 2003-2007, Alexander Iliev <sasho@cs.dartmouth.edu> and
 * Sean W. Smith <sws@cs.dartmouth.edu>
 *
 * All rights reserved.
 *
 * This code is released under a BSD license.
 * Please see LICENSE.txt for the full license and disclaimers.
 *
 */

#include "batcher-network.h"

#include <pir/common/function-objs.h>

#include <time.h>

#include <algorithm>
#include <iostream>
#include <iomanip>

#include <assert.h>


using namespace std;


vector<unsigned> V;

void swap_V (index_t a, index_t b)
{
    if (V[a] > V[b])
    {
	// use vector::at() so it throws on out-of-range index.
	std::swap (V.at(a), V.at(b));
    }
}


void print_comp_indices (index_t a, index_t b)
{
    cout << setw(4) << a << setw(4) << b << endl;
}




int main (int argc, char * argv[])
{

    assert ( ("Need a command-line param for N",
	      argc > 1) );
    
    size_t N = atoi (argv[1]);

    // print out the indices being compared.
//    run_batcher (N, print_comp_indices, NULL);


    // and do a sorting test
    
    V.resize(N);

    generate (V.begin(), V.end(), counter(N*7 + 98));

    // do a Knuth-style in-place random permutation, with non-too-serious
    // randomness.
    srandom (time(NULL));
    for (index_t i = N-1; i > 0; i--) {
	int j = random () % (i+1);
	std::swap (V[i], V[j]);
    }
    
    run_batcher (N, swap_V, NULL);

    // check if sorted.
    for (unsigned i=0; i < N-1; i++)
    {
	if (V[i] > V[i+1])
	{
	    cerr << "** error at i=" << i << endl;
	}
    }
}
