// -*- c++ -*-
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

#include <boost/optional/optional.hpp>

#include <faerieplay/common/utils.h>
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
