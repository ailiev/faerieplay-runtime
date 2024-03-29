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

#include <faerieplay/common/exceptions.h>

#include <string>

class illegal_operation_argument : public better_exception
{
public:
    illegal_operation_argument (const std::string& msg)
	: better_exception (msg)
	{}
};

