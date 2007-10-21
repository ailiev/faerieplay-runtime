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
    
#include <istream>
#include <string>

#include <pir/common/sym_crypto.h>

int prepare_gates_container (std::istream & gates_in,
			     const std::string& cct_name,
			     CryptoProviderFactory * crypto_fact)
    throw (io_exception, bad_arg_exception,  std::exception);


