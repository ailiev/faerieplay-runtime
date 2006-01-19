// -*- c++ -*-

#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/common/comm_types.h> // for index_t
				   // FIXME: this is silly


#ifndef _SFDL_CARD_UTILS_H
#define _SFDL_CARD_UTILS_H


int hostio_read_int (FlatIO & io, index_t idx);

void hostio_write_int (FlatIO & io, index_t idx,
		       int val);


#endif // _SFDL_CARD_UTILS_H
