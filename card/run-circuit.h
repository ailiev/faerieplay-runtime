// -*- c++ -*-
/*
Copyright (C) 2003-2007, Alexander Iliev <sasho@cs.dartmouth.edu>

All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.
* Neither the name of the author nor the names of its contributors may
  be used to endorse or promote products derived from this software without
  specific prior written permission.

This product includes cryptographic software written by Eric Young
(eay@cryptsoft.com)

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <string>

#include <boost/optional/optional.hpp>

#include <pir/common/utils.h>
#include <pir/common/sym_crypto.h>
#include <pir/card/io_flat.h>

#include <common/gate.h>


#ifndef _RUN_CIRCUIT_H
#define _RUN_CIRCUIT_H


OPEN_NS


class CircuitEval
{

public:

    /// Create an evaluator, giving it the name of a circuit created by
    /// prep-circuit.cc
    CircuitEval (const std::string& cctname,
		 CryptoProviderFactory * fact);

    /// Evaluate the circuit!
    void eval ();


private:

    void do_gate (const gate_t& g);
    
    /// Read the gate at the given circuit step.
    void read_gate_at_step (gate_t & o_gate,
			    int step_num);

    /// read the gate of the given number.
    void read_gate (gate_t & o_gate,
		    int gate_num);

    /// a helper for the above two which does the real work when given a
    /// container to read from.
    void read_gate_helper (FlatIO & io,
			   gate_t & o_gate,
			   int num);
	
    /// @return the descriptor of the resulting array
    ByteBuffer do_read_array (bool enable,
			      const ByteBuffer& arr_ptr,
			      boost::optional<index_t> idx,
			      ByteBuffer & o_val);

    /// @return the descriptot of the resulting array
    ByteBuffer do_write_array (bool enable,
			       const ByteBuffer& arr_ptr_buf,
			       size_t off,
			       boost::optional<size_t> len,
			       boost::optional<index_t> idx,
			       const ByteBuffer& new_val);

    /// get the current value at this gate's output
    boost::optional<int> get_int_val (int gate_num);

    std::string get_string_val (int gate_num);

    ByteBuffer get_gate_val (int gate_num);
    void put_gate_val (int gate_num, const ByteBuffer& val);
    
    /// Log the gate value to the values log
    void log_gate_value (const gate_t& g, const ByteBuffer& val);
    
    FlatIO
    _gates_io,			// the gates s.t. gate number g is at _gates_io[g]
	_cct_io,		// the gates in (topological) order of execution
	_vals_io;		// values, s.t. val of gate g is at _vals_io[g]

    CryptoProviderFactory * _prov_fact;

public:

    static Log::logger_t logger, gate_logger;

    DECL_STATIC_INIT (
	logger = Log::makeLogger ("circuit-vm.card.run-circuit");

#ifdef LOGVALS
	// since the use of gate_logger is controlled by another macro, can
	// have it log up to a very high priority.
	gate_logger = Log::makeLogger ("circuit-vm.card.gate-logger");
#endif

	);
};


DECL_STATIC_INIT_INSTANCE(CircuitEval);






CLOSE_NS


#endif // _RUN_CIRCUIT_H
