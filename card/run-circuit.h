// -*- c++ -*-

#include <string>

#include <boost/optional/optional.hpp>

#include <pir/common/utils.h>
#include <pir/common/sym_crypto.h>
#include <pir/card/io_flat.h>

#include <common/gate.h>


#ifndef _RUN_CIRCUIT_H
#define _RUN_CIRCUIT_H

class CircuitEval
{

public:

    CircuitEval (const std::string& cctname,
		 CryptoProviderFactory * fact);

    void eval ();


private:

    void do_gate (const gate_t& g);
    
    void read_gate (gate_t & o_gate,
		    int gate_num);

    ByteBuffer do_read_array (const ByteBuffer& arr_ptr,
			      index_t idx);

    void do_write_array (const ByteBuffer& arr_ptr_buf,
			 size_t off,
			 boost::optional<size_t> len,
			 index_t idx,
			 const ByteBuffer& new_val,
			 int prev_depth, int this_depth);

    /// get the current value at this gate's output
    int get_int_val (int gate_num);

    std::string get_string_val (int gate_num);

    ByteBuffer get_gate_val (int gate_num);
    void put_gate_val (int gate_num, const ByteBuffer& val);
    
    
    FlatIO _cct_io;
    FlatIO _vals_io;
};


#endif // _RUN_CIRCUIT_H
