/*
 * Circuit virtual machine for the Faerieplay hardware-assisted secure
 * computation project at Dartmouth College.
 *
 * Copyright (C) 2003-2007, Alexander Iliev <alex.iliev@gmail.com> and
 * Sean W. Smith <sws@cs.dartmouth.edu>
 *
 * All rights reserved.
 *
 * This code is released under a BSD license.
 * Please see LICENSE.txt for the full license and disclaimers.
 *
 */


#include <vector>
#include <algorithm>

#include <boost/bind.hpp>

#include "processor.h"
#include "helpers.h"

#include <pir/card/io_flat.h>


using namespace pir;

void shortening_itemproc (index_t i,
			  const ByteBuffer& in,
			  ByteBuffer & out)
{
    out = in;
    out.len() -= 1;
}

void shortening_2_itemproc (const boost::array<index_t,2>& idxs,
			    const boost::array<ByteBuffer,2>& in,
			    boost::array<ByteBuffer,2>& out)
{
    out[0] = in[0];
    out[1] = in[1];
    out[0].len() -= 3;
    out[1].len() -= 3;
}


int main (int argc, char *argv[])
{
    using namespace std;
    using namespace boost;
    
    const size_t N = 64;
    const size_t elem_size = 16;
    
    vector<ByteBuffer> to_write (N);
    vector<index_t> idxs(N);
    vector<ByteBuffer> to_read (N);

    FlatIO cont1 ("stream-tester-1",
		  Just (make_pair(N, elem_size)));

    FlatIO cont2 ("stream-tester-2",
		  Just (make_pair(N, elem_size)));

    FlatIO cont3 ("stream-tester-3",
		  Just (make_pair(N, elem_size)));


    std::generate (idxs.begin(), idxs.end(),
		   counter (0));

    // fill string in with  strings corresponding to a sequence of ints starting
    // from 12345678
    std::generate ( to_write.begin(), to_write.end(),
		    bind (string2bb, bind (itoa, bind (counter (12345678)))) );

    cout << "initial value of to_write" << endl;
    
    copy (to_write.begin(), to_write.end(),
	  ostream_iterator<ByteBuffer>(cout, "\n"));

    cont1.write (idxs, to_write);

    stream_process (identity_itemproc<>(),
		    zero_to_n (N),
		    &cont1,
		    &cont2);

    array<FlatIO*, 2> ins = { &cont1, &cont2 };
    stream_process (shortening_2_itemproc,
		    zero_to_n<2> (N),
		    ins,
		    ins,
		    boost::mpl::size_t<1>(),
		    boost::mpl::size_t<2>());

//     stream_process (shortening_itemproc,
// 		    zero_to_n (N),
// 		    &cont1,
// 		    &cont3);

    cont2.read (idxs, to_read);

    cout << "After applyitng shortening_2_itemproc: " <<  endl;
    copy (to_read.begin(), to_read.end(),
	  ostream_iterator<ByteBuffer>(cout, "\n"));
}
