// -*- c++ -*-

#include <pir/common/exceptions.h>

#include <string>

class illegal_operation_argument : public better_exception
{
public:
    illegal_operation_argument (const std::string& msg)
	: better_exception (msg)
	{}
};

