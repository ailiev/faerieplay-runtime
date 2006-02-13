#include <vector>
#include <algorithm>

#include <pir/common/exceptions.h>

#include "utils.h"




int bb2int (const ByteBuffer& bb)
{
    int answer = 0;
    if (bb.len() != sizeof(answer)) {
	throw (bad_arg_exception (
		   "ByteBuffer with an alleged int is not 4 bytes long"));
    }
    memcpy (&answer, bb.data(), bb.len());
    return answer;
}


// read an integer from this io object
int hostio_read_int (FlatIO & io, index_t idx)
{
    ByteBuffer buf;
    io.read (idx, buf);
    return bb2int (buf);
}

#if 0
void hostio_read_ints (FlatIO & io,
    const FlatIO::idx_list_t & idxs,
    std::vector<int> & o_ints)
    throw (better_exception)
{
    obj_list_t objs;
    io.read (idxs, objs);
    transform (objs.begin(), objs.end(),
	       o_ints.begin(),
	       bb2int);
}

void hostio_write_ints (FlatIO & io,
			const FlatIO::idx_list_t & idxs,
			const std::vector<int> & ints)
    throw (better_exception)
{
    obj_list_t objs (ints.size());
    transform (ints.begin(), ints.end(),
	       objs.begin(),
	       int2bb);
	       
    io.write (idxs, objs);
}
#endif // 0


ByteBuffer int2bb (int i)
{
    ByteBuffer answer (sizeof (i));
    memcpy (answer.data(), &i, answer.len());
    return answer;
}


void hostio_write_int (FlatIO & io, index_t idx,
		       int val)
{
    ByteBuffer buf (&val, sizeof(val),
		    ByteBuffer::SHALLOW);

    io.write (idx, buf);
}


