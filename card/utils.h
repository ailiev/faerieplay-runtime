// -*- c++ -*-

#include <pir/card/io.h>
#include <pir/common/comm_types.h> // for index_t
				   // FIXME: this is silly


#ifndef _SFDL_CARD_UTILS_H
#define _SFDL_CARD_UTILS_H


int hostio_read_int (HostIO & io, index_t idx);

void hostio_write_int (HostIO & io, index_t idx,
		       int val);


#endif // _SFDL_CARD_UTILS_H
