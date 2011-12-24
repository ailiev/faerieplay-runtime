// -*- c++ -*-
/*
 * Circuit virtual machine for the Faerieplay hardware-assisted secure
 * computation project at Dartmouth College.
 *
 * Copyright (C) 2003-2007, Alexander Iliev <alex.iliev@gmail.com> and
 * Sean W. Smith <sws@cs.dartmouth.edu>
 *
 * All rights reserved.
 *
 * This code is released under a BSD license.
 * Please see LICENSE.txt for the full license and disclaimers.
 *
 */

#include <string>

// here we'll put the gates in topological order
const std::string CCT_CONT = "circuit";

// here gates are in their numbered slot
const std::string GATES_CONT = "gates";

//and here are gate values
const std::string VALUES_CONT = "values";

const std::string ENC_KEY_FILE = "enc.key";
const std::string MAC_KEY_FILE = "mac.key";

