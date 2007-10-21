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


#include <signal.h>		// for raise()
#include <iostream>
void out_of_memory_coredump ()
{
    std::cerr << "Out of memory!" << std::endl;
    // trigger a SEGV, so we can get a core dump - useful when running on the
    // 4758 when we can't debug interactively, but do get core dumps.	    
    // * ((int*) (0x0)) = 42;

    // or, trap into the debugger.
    raise (SIGTRAP);

    // re-start process failure due to failed alloc.
    throw std::bad_alloc();
}
