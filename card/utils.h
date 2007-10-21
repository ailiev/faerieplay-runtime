// -*- c++ -*-
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

#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/common/comm_types.h> // for index_t
				   // FIXME: this is silly


#ifndef _SFDL_CARD_UTILS_H
#define _SFDL_CARD_UTILS_H


void prefetch_contiguous_from (FlatIO & io,
			       index_t start,
			       size_t howmany)
    throw (better_exception);

void standard_prefetch (FlatIO & io,
			index_t i,
			size_t batch_size,
			size_t total)
    throw (better_exception);


int hostio_read_int (FlatIO & io, index_t idx);

void hostio_write_int (FlatIO & io, index_t idx,
		       int val);

///
/// Convenient handler for memory exhaustion, which causes a core dump or some
/// such hook to enable analysis.
void out_of_memory_coredump ();



#endif // _SFDL_CARD_UTILS_H
