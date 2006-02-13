#include <vector>
#include <algorithm>

#include <boost/bind.hpp>

#include "processor.h"
#include "helpers.h"

#include <pir/card/io_flat.h>

void shortening_itemproc (index_t i,
			  const ByteBuffer& in,
			  ByteBuffer & out)
{
    out = in;
    out.len() -= 1;
}


int main (int argc, char *argv[])
{
    using namespace std;
    using namespace boost;
    
    const size_t N = 64;
    
    vector<string> to_write (N);
    vector<index_t> idxs(N);
    vector<string> to_read (N);

    FlatIO cont1 ("stream-tester-1",
		  Just (make_pair(N, 16U)));

    FlatIO cont2 ("stream-tester-2",
		  Just (make_pair(N, 16U)));

    FlatIO cont3 ("stream-tester-3",
		  Just (make_pair(N, 16U)));


    std::generate (idxs.begin(), idxs.end(),
		   counter (0));

    // fill string in with  strings corresponding to a sequence of ints starting
    // from 12345678
    std::generate ( to_write.begin(), to_write.end(),
		    bind (itoa, bind (counter (12345678))) );

    cont1.write (idxs, to_write);

    stream_process (identity_itemproc,
		    zero_to_n (N),
		    &cont1,
		    &cont2);

    stream_process (shortening_itemproc,
		    zero_to_n (N),
		    &cont1,
		    &cont3);
}
