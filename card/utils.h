// -*- c++ -*-

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


#endif // _SFDL_CARD_UTILS_H
