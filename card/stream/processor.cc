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

#include "processor.h"

#include <faerieplay/common/logging.h>
#include <faerieplay/common/utils-macros.h>


OPEN_NS

Log::logger_t StreamProcessor::logger; // declared extern in processor.h

INSTANTIATE_STATIC_INIT (StreamProcessor);

CLOSE_NS
