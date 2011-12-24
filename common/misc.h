#include <string>
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

#include <stdint.h>


typedef uint32_t arr_ptr_t;

arr_ptr_t make_array_pointer (char arr_name, int generation);

std::string make_array_name (arr_ptr_t ptr);

std::string make_array_container_name (arr_ptr_t ptr);


