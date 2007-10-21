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


// program to encrypt a circuit object on the host. the circuit was produced in
// cleartext by "prep-circuit.cc", and we want to MAC it before running it.
// Since the SymWrapper class does enc and MAC together, we'll just do both.
//
// reads from the container CCT_CONT, and writes the encrypted values back in
// there.

#include "enc-circuit.h"

#include <string>
#include <stdexcept>

#include <iostream>

#include <boost/shared_ptr.hpp>
#include <boost/optional/optional.hpp>
#include <boost/none.hpp>


#include <pir/card/io.h>
#include <pir/card/io_flat.h>
#include <pir/card/lib.h>
#include <pir/card/configs.h>
#include <pir/card/io_filter.h>
#include <pir/card/io_filter_encrypt.h>

#include <pir/common/comm_types.h>
#include <pir/common/sym_crypto.h>


#include <common/gate.h>
#include <common/misc.h>
#include <common/consts-sfdl.h>

#include "stream/processor.h"
#include "stream/helpers.h"


using namespace std;
using namespace pir;

using boost::shared_ptr;


namespace
{
    Log::logger_t logger = Log::makeLogger ("circuit-vm.card.enc-circuit");
}





void do_encrypt (CryptoProviderFactory* crypt_fact)
    throw (std::exception)
{
    //
    // and do the work ...
    //

    FlatIO  vals_io (g_configs.cct_name + DIRSEP + VALUES_CONT, boost::none);
    FlatIO  cct_io  (g_configs.cct_name + DIRSEP + CCT_CONT, boost::none);

    ByteBuffer obj_bytes;

    FlatIO *conts[] = { &vals_io, &cct_io };
	
    // go through all the containers
    for (unsigned c = 0; c < ARRLEN(conts); c++) {
	    
//	    clog << "*** Working on container " << io->getName() << endl;
	    
	    
	FlatIO * io = conts[c];
	    
	size_t num_objs = io->getLen();
	    
	// temp container to take encrypted objects
	FlatIO temp (io->getName() + "-enc",
		     make_pair (io->getLen(), io->getElemSize()));

	temp.appendFilter (auto_ptr<HostIOFilter>
			   (new IOFilterEncrypt (&temp,
						 shared_ptr<SymWrapper>
						 (new SymWrapper
						  (crypt_fact)))));

	// and transfer each object in the container
	stream_process ( identity_itemproc<>(),
			 zero_to_n (num_objs),
			 io,
			 &temp );

	// and move encrypted container back to original name.
	*io = temp;
    }
}
