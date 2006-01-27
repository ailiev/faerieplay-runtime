#include <string>

#include <stdint.h>


typedef uint32_t arr_ptr_t;

arr_ptr_t make_array_pointer (char arr_name, int generation);

std::string make_array_name (arr_ptr_t ptr);

std::string make_array_container_name (arr_ptr_t ptr);


