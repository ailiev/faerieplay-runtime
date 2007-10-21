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


#include <string>

#include <pir/common/utils.h>

#include "misc.h"

using namespace std;



arr_ptr_t make_array_pointer (char arr_name, int generation) {
    arr_ptr_t answer;

    // put the name in the low byte, and the generation in the N-1 high bytes
    answer = (generation << 8) | arr_name;

    return answer;
}


void split_array_ptr (char * o_name, int * o_gen, arr_ptr_t ptr) {
    if (o_name) *o_name = (ptr & 0xFF);
    if (o_gen)  *o_gen  = (ptr >> 8);
}

std::string make_array_container_name (arr_ptr_t ptr) {

    char arr_str[] = { (ptr & 0xFF), '\0' };
    int gen = (ptr >> 8);
    
    return "array-" + string(arr_str) + "-" + itoa(gen);

}

