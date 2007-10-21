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

#include <pir/common/sym_crypto.h>

#include <exception>

void do_encrypt (CryptoProviderFactory* crypt_fact)
    throw (std::exception);

