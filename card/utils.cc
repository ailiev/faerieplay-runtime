#include "utils.h"

// read an integer from this io object
int hostio_read_int (FlatIO & io, index_t idx)
{

    int answer;
    
    ByteBuffer buf;
    io.read (idx, buf);

    assert (buf.len() == sizeof(answer));
    
    memcpy (&answer, buf.data(), sizeof(answer));
    return answer;
}


void hostio_write_int (FlatIO & io, index_t idx,
		       int val)
{
    ByteBuffer buf (&val, sizeof(val),
		    ByteBuffer::no_free);

    io.write (idx, buf);
}

