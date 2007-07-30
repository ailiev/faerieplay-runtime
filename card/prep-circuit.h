// -*- c++ -*-

#include <istream>
#include <string>

#include <pir/common/sym_crypto.h>

int prepare_gates_container (std::istream & gates_in,
			     const std::string& cct_name,
			     CryptoProviderFactory * crypto_fact)
    throw (io_exception, bad_arg_exception,  std::exception);


