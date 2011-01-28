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

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utility>

#include <boost/optional/optional.hpp>

//#include <pir/host/objio.h>
#include <faerieplay/common/utils.h>
#include <pir/common/consts.h>

#include <pir/card/configs.h>
#include <pir/card/io_flat.h>
#include <faerieplay/common/logging.h>

#include <common/consts-sfdl.h>
#include <common/gate.h>
#include <common/misc.h>

#include <json/get-path.h>
// #include <json/bnfc/Parser.H>

#include "array.h"

// for stdin. wanted to use cstdio here, but it does not define std::stdin
// apparently.
#include <stdio.h>	


using namespace std;

using pir::Array;
using pir::ArrayHandle;

using boost::optional;



int prepare_gates_container (istream & gates_in,
			     const string& cct_name)
    throw (io_exception, bad_arg_exception, std::exception);



namespace
{
    Log::logger_t logger = Log::makeLogger ("circuit-vm.card.prep-circuit");
}




/// Convenience function to find an input array, squash it, and thrown an
/// exception if it is missing.
namespace
{
    vector< vector<int> > get_input_array (PathFinder & in_data,
					   const vector<string>& path)
	throw (bad_arg_exception)
    {
	optional<vector<Value*> > opt_list = in_data.findList (path);

	if (!opt_list) {
	    string name;
	    for (unsigned i=0; i<path.size(); ++i) {
		name += path[i] + ".";
	    }
	    throw bad_arg_exception ("Could not find the input array named "
				     + name + " in the input data provided");
	}

	return PathFinder::squashList (*opt_list);
    }
}
					   

const size_t CONTAINER_OBJ_SIZE = 128;


int prepare_gates_container (istream & gates_in,
			     const string& cct_name,
			     CryptoProviderFactory * crypto_fact)
    throw (io_exception, bad_arg_exception,  std::exception)
{

    string line;
    unsigned gate_num = 0;
    unsigned max_gate = 0;
    ostringstream gate;
    vector< pair<index_t,string> > gates;

    ByteBuffer zeros (CONTAINER_OBJ_SIZE);
    zeros.set (0);

    string
	cct_cont    = cct_name + DIRSEP + CCT_CONT,
	gates_cont  = cct_name + DIRSEP + GATES_CONT,
	values_cont = cct_name + DIRSEP + VALUES_CONT;


    // read in all the gates
    while (true) {
	    
	// get the gate number
	if (gates_in >> gate_num) {
	    gate << gate_num << endl;
	    max_gate = max (max_gate, gate_num);
	    
	    // finish off this line
	    getline (gates_in, line);
	    LOG (Log::DEBUG, logger,
		 "gate " << gate_num);
	}
	else if (gates_in.eof()) {
	    break;		// done
	}
	else {
	    // probably just a blank line
	    continue;
	}
	    
	/// and the rest of the gate lines
	while (getline (gates_in, line) && line != "") {
	    LOG (Log::DEBUG, logger,
		 "line: " << line);
	    gate << line << endl;
	}

	LOG (Log::DEBUG, logger,
	     "gate done");

	gates.push_back ( make_pair (gate_num, gate.str()) );
	    
	gate.str("");
    }
	
    LOG (Log::PROGRESS, logger,
	 "Done reading circuit");
    


    // create and fill in the containers
    
    FlatIO
	io_cct	    (cct_cont,
		     Just (make_pair (gates.size(), CONTAINER_OBJ_SIZE))),
	io_gates    (gates_cont,
		     Just (make_pair (max_gate+1, CONTAINER_OBJ_SIZE))),
	io_values   (values_cont,
		     Just (make_pair (max_gate+1, CONTAINER_OBJ_SIZE)));
    
    LOG (Log::INFO, logger,
	 "cct_cont size=" << gates.size()
	 << "; gates_cont size=" << max_gate + 1);


    // prepare the input extractor, from stdin. This will throw an exception on
    // a parse error.
    PathFinder in_vals (stdin);

    LOG (Log::PROGRESS, logger, "Parsed inputs from stdin");

    index_t i = 0;
    FOREACH (g, gates) {
	gate_t gate = unserialize_gate (g->second);

	LOG (Log::DEBUG, logger,
	     "Processing gate number " << gate.num);

	if (gate.op.kind == gate_t::Input) {

	    //
	    // get the input
	    //
	    const string& input_name = gate.comment;
	    const vector<string> input_path = split (".", input_name);
	    
	    switch (gate.typ.kind)
	    {
	    case gate_t::Scalar:
	    {
		LOG (Log::INFO, logger,
		     "Obtaining scalar input " << input_name
		     << " for gate " << gate.num);

		optional<int> val = in_vals.find (input_path);
		if (!val) {
		    const string msg = "Could not locate input named '" + input_name;
		    LOG (Log::CRIT, logger, msg);
		    throw bad_arg_exception (msg);
		}
		
		ByteBuffer val_buf = optBasic2bb<int> (val);
		
		LOG (Log::INFO, logger,
		     "writing " << val_buf.len() << " byte input value");
		
		io_values.write (gate.num, val_buf);
	    }
	    break;

	    case gate_t::Array:
	    {
		size_t length	= gate.typ.params[0],
		    elem_size	= gate.typ.params[1];

		// NOTE: use the gate comment for the array's name, the runtime
		// has to do the same.
		const string& arr_cont_name = input_name;

		string in_str;
		
		const size_t num_components = elem_size / OPT_BB_SIZE(int);

		// create the Array object to use to write. It will encrypt
		// before writing out.
		Array arr (arr_cont_name,
			   Just (make_pair (length, elem_size)),
			   crypto_fact);

		assert (((void) "Element size must be a multiple of the byte size "
			 "of optional<int>",
			 elem_size % OPT_BB_SIZE(int) == 0));
		    
		//
		// collect all the values from the input data object.
		//
		
		vector <vector<int> > vals =
		    get_input_array (in_vals, input_path);

		if (vals.size() != length) {
		    ostringstream os;
		    os << "The input provided for array " << input_name
		       << " should have " << length << " elements, but actually has "
		       << vals.size() << ends;
// TODO: make this a runtime check, based on some cmd line param or env
// variable.
#if STRICT_INPUT_ARRAY_LEN
		    throw bad_arg_exception (os.str());
#else
		    LOG (Log::WARN, logger,
			 os.str() << ", will use nil for the missing values");
#endif
		}
		
		for (unsigned l_i = 0; l_i < length; l_i++)
		{
		    
		    // ASSUME: the array elements, per the SFDL program, are all
		    // 32-bit integers, or structs of them. We do not supported
		    // nested arrays currently.

		    if (l_i < vals.size() && vals[l_i].size() != num_components) {
			ostringstream os;
			os << "The input provided for array " << input_name
			   << " element " << l_i
			   << " should have " << num_components
			   << " components, but actually has "
			   << vals[i].size() << ends;
			throw bad_arg_exception (os.str());
		    }
		    
		    // get all the array element components, or nil's if we have
		    // run out of input elements.
		    ByteBuffer ins_buf (elem_size);
		    for (unsigned i=0; i < num_components; i++)
		    {
			ByteBuffer member(OPT_BB_SIZE(int));

			// if we have no more input values
			if (l_i >= vals.size()) {
			    // insert nil values
			    makeOptBBNothing (member);
			}
			else {
			    // insert bytes for the i'th component into the buffer
			    // for the l_i'th array element.
			    int * val = & vals[l_i][i];
			    makeOptBBJust (member, val, sizeof (*val));
			
			    LOG (Log::DEBUG, logger,
				 "Writing int " << (*val)
				 << ", bytebuffer " << member
				 << " at idx " << l_i << " of array " << arr.name());
			}
			    
			// an alias at the correct offset of ins_buf
			ByteBuffer member_dest (ins_buf,
						i*OPT_BB_SIZE(int),
						OPT_BB_SIZE(int));
			
			bbcopy (member_dest, member);
		    }

		    // write the array value into the array container
		    arr.write_clear (l_i, 0, ins_buf);
		}

		// and write a blank value of the right size into the values
		// table. the runtime will load a handle to the array and provide
		// that handle as the actual value for this gate.
                io_values.write (gate.num,
				 ByteBuffer (sizeof(ArrayHandle::des_t)));
		
	    } // end case Array:
	    break;

	    } // end switch (gate.typ.kind)

	} // end if (gate.op.kind == gate_t::Input)
	else {
	    // we should enter something into the values container
	    io_values.write (gate.num, zeros);
	}
	
	// write the string form of the gate into the two containers.
	ByteBuffer gatestring = ByteBuffer (g->second, ByteBuffer::SHALLOW);

	io_gates.write (g->first, gatestring);
	io_cct.write   (i++,      gatestring);

    } // end FOREACH (g, gates)
	
    return max_gate;
}

