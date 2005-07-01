#include <string>
#include <list>
#include <functional>
#include <stdexcept>

#include <iostream>
#include <sstream>
#include <fstream>

#include <pir/card/hostcall.h>
#include <pir/card/4758_sym_crypto.h>

#include <pir/common/comm_types.h>
#include <pir/common/sym_crypto.h>

#include <common/gate.h>
#include <common/misc.h>
#include <common/consts-sfdl.h>

using namespace std;




void do_gate (const gate_t& gate)
    ;

void do_unwrap (const ByteBuffer& val)
    ;


int get_int_val (int gate_num)
    ;
string get_string_val (int gate_num)
    ;
ByteBuffer get_gate_val (int gate_num)
    ;

void put_gate_val (int gate_num, const ByteBuffer& val)
    ;


ByteBuffer do_read_array (const ByteBuffer& arr_ptr,
			  int idx)
    ;

void do_write_array (const ByteBuffer& arr_ptr,
		     int off, int len,
		     int idx,
		     const ByteBuffer& new_val)
    ;


void read_gate (gate_t & o_gate,
		int gate_num)
    ;



static void exception_exit (const std::exception& ex, const string& msg) {
    cerr << msg << ":" << endl
	 << ex.what() << endl
	 << "Exiting ..." << endl;
    exit (EXIT_FAILURE);
}


SymWrapper * g_symrap = NULL;

int main (int argc, char *argv[]) {

    
    ByteBuffer gate_bytes, val;
    string gate_str;
    gate_t gate;

    // shut up clog for now
//    ofstream out("/dev/null");
//    if (out)
    clog.rdbuf(NULL);

    size_t num_gates = host_get_cont_len (CCT_CONT);


    //
    // crypto setup, for now just used to provide the correct timings
    //
    ByteBuffer mac_key (20),	// SHA1 HMAC key
	enc_key (24);		// TDES key

    auto_ptr<SymCryptProvider>	symop;
    auto_ptr<MacProvider>		macop;
    auto_ptr<CryptoProviderFactory>	provfact;

    try {
	symop.reset (new SymCrypt4758 ());
	macop.reset (new SHA1HMAC_4758());
	provfact.reset (new CryptProvFactory4758());
    }
    catch (const crypto_exception& ex) {
	exception_exit (ex, "Error making crypto providers");
    }

    SymWrapper symrap (enc_key, *symop, mac_key, *macop);
    g_symrap = &symrap;


    
    try {
	for (unsigned i=0; i < num_gates; i++) {

	    if (i % 100 == 0) {
		cerr << "Doing gate " << i << endl;
	    }

	    read_gate (gate, i);

//	    print_gate (clog, gate);

	    do_gate (gate);

	}
    }
    catch (const std::exception & ex) {
	cerr << "Exception: " << ex.what() << endl;
	exit (EXIT_FAILURE);
    }
    


}


void read_gate (gate_t & o_gate,
		int gate_num)
    
{
    ByteBuffer gate_bytes;
    
    host_read_blob (CCT_CONT,
		    object_name_t (object_id (gate_num)),
		    gate_bytes);

    // this just needs to be a MAC check, but a decrypt should take a similar
    // time
    do_unwrap (gate_bytes);

// 	clog << "gate_byte len: " << gate_bytes.len() << endl;
// 	    "bytes: " << gate_bytes.cdata() <<  endl;

	    
    o_gate = unserialize_gate (string(gate_bytes.cdata(),
				      gate_bytes.len()));
}

void do_gate (const gate_t& g)
    
{
    int res;
    ByteBuffer res_bytes;


    switch (g.op.kind) {

    case BinOp:
    {
	int arg_val[2];
	
	assert (g.inputs.size() == 2);
	for (int i=0; i < 2; i++) {
	    arg_val[i] = get_int_val (g.inputs[i]);
	}

	res = do_bin_op (static_cast<binop_t>(g.op.params[0]),
			 arg_val[0], arg_val[1]);

	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::no_free);

    }
    break;

    
    case UnOp:
    {
	int arg_gate, arg_val;
	
	assert (g.inputs.size() == 1);
	arg_gate = g.inputs[0];
	arg_val = get_int_val (arg_gate);

	res = do_un_op (static_cast<unop_t>(g.op.params[0]),
			arg_val);
	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::no_free);
	
    }
    break;

    
    case Input:
	// already been done for us, go on
	break;

    case Lit:
	// place the lit value into the slot
	res = g.op.params[0];
	res_bytes = ByteBuffer (&res, sizeof(res), ByteBuffer::no_free);

	break;


    case Select:
    {
	ByteBuffer input_vals[2];
	int selector;

	assert (g.inputs.size() == 3);
	selector = get_int_val (g.inputs[0]);
	for (int i=0; i < 2; i++) {
	    input_vals[i] = get_gate_val (g.inputs[i+1]);
	}

	// it is really a selector!
	res_bytes = selector ? input_vals[0] : input_vals[1];
    }	
    break;

    
    case ReadDynArray:
    {
	ByteBuffer arr_ptr = get_gate_val (g.inputs[0]);
	int idx = get_int_val (g.inputs[1]);
	
	ByteBuffer read_res = do_read_array (arr_ptr, idx);

	ByteBuffer outs [] = {  arr_ptr, read_res };
	res_bytes = concat_bufs (outs, outs + ARRLEN(outs));
    }
    break;

/*
    case WriteDynArray:
    {
	int off = g.op.params[0];
	int len = g.op.params[1];

	ByteBuffer arr_ptr = get_gate_val (g.inputs[0]);
	int idx = get_int_val (g.inputs[1]);


	// and load up the rest of the inputs into vals
	vector<ByteBuffer> vals (g.inputs.size()-2);
	transform (g.inputs.begin()+2,
		   g.inputs.end(),
		   vals.begin(),
		   get_gate_val);

	// concatenate the inputs
	ByteBuffer ins = concat_bufs (vals.begin(), vals.end());

	// read in the whole array gate to see what depth it was at
	gate_t arr_gate;
	read_gate (arr_gate, arr_gate_num);



	do_write_array (arr_ptr, idx, off, len, ins,
			arr_gate.depth, g.depth);
    }
    break;
*/

    case Slicer:
    {
	int off, len;

	off = g.op.params[0];
	len = g.op.params[1];

	ByteBuffer val = get_gate_val (g.inputs[0]);
	ByteBuffer out (len);

	assert (val.len() >= off + len);
	memcpy (out.data(), val.data() + off, len);

	res_bytes = out;
	
    }
    break;
	
    default:
	cerr << "At gate " << g.num
	     << ", unknown operation " << g.op.kind << endl;
	exit (EXIT_FAILURE);

    }

    
    if (res_bytes.len() > 0) {
	put_gate_val (g.num, res_bytes);
    }
    

    if (elem (Output, g.flags)) {
	int intval;
	assert (res_bytes.len() == sizeof(intval));
	memcpy (&intval, res_bytes.data(), sizeof(intval));
	cout << "Output " << g.comment << ": " << intval << endl;
    }
}





void put_gate_val (int gate_num, const ByteBuffer& val)
    
{
    host_write_blob (VALUES_CONT,
		     object_name_t (object_id (gate_num)),
		     val);
}


ByteBuffer get_gate_val (int gate_num)
    
{

    ByteBuffer buf;
    
    host_read_blob (VALUES_CONT,
		    object_name_t (object_id (gate_num)),
		    buf);

    do_unwrap (buf);


    clog << "get_gate_val for gate " << gate_num
	 << ": len=" << buf.len() << endl;
    
    return buf;
}


// get the current value at this gate's output
int get_int_val (int gate_num)
    
{

    int answer;
    
    ByteBuffer buf = get_gate_val (gate_num);

    assert (buf.len() == sizeof(answer));
    
    memcpy (&answer, buf.data(), sizeof(answer));
    return answer;
}


string get_string_val (int gate_num)
    
{

    ByteBuffer buf = get_gate_val (gate_num);

    return string (buf.cdata(), buf.len());
}


ByteBuffer do_read_array (const ByteBuffer& arr_ptr,
			  int idx)
    
{
    arr_ptr_t ptr;

    static int arr_len = -1;
    static unsigned workset_len;
    static unsigned req_num = 0;

    static unsigned BATCHSIZE = 0;
    
    assert (arr_ptr.len() == sizeof(ptr));
    memcpy (&ptr, arr_ptr.data(), sizeof(ptr));

    string cont = make_array_container_name (ptr);

    if (arr_len < 0) {
	arr_len = host_get_cont_len (cont);
	workset_len = sqrt(float(arr_len)) * lgN_floor(arr_len);
	BATCHSIZE = MIN (workset_len/4, 64);
	cerr << "BATCHSIZE = " << BATCHSIZE << endl;
    }

    ByteBuffer val;

    
    // some extra reads here to simulate going through the working area.
    for (int i = 0; i < (workset_len/BATCHSIZE); i++) {
//	object_name_t name (object_id(idx));
	std::vector<object_name_t> names (BATCHSIZE,
					  object_name_t (object_id(idx)));
	obj_list_t results;
	
	host_read_blobs (cont,
			 names,
			 results);

    }

    // and the real read
    host_read_blob (cont,
		    object_name_t (object_id(idx)),
		    val);
    
    if (req_num++ > workset_len) {

	req_num = 0;
	
	// reshuffle!
	cerr << "Reshuffle!" << endl;
	
	unsigned num_switches =
	    arr_len *
	    lgN_floor(arr_len) *
	    lgN_floor(arr_len) / 4;
	
	ByteBuffer buf1, buf2;
	
	for (int i = 0; i < num_switches*2 / BATCHSIZE; i++) {
	    object_name_t name (object_id(16));
	    std::vector<object_name_t> names (BATCHSIZE, name);
	    obj_list_t results;
	    
	    host_read_blobs (cont,
			     names,
			     results);

	    host_write_blobs (cont,
			      names,
			      results);
	}

	cerr << "Reshuffle done!" << endl;
    }
    
    return val;
}

/*
void do_write_array (const ByteBuffer& arr_ptr,
		     int off, int len,
		     int idx,
		     const ByteBuffer& new_val,
		     int prev_depth, int this_depth)
    
{
    arr_ptr_t ptr;
    char arr_name;
    int arr_depth;

    assert (len == new_val.len());
    
    assert (arr_ptr.len() == sizeof(ptr));
    memcpy (&ptr, arr_ptr.data(), sizeof(ptr));

    string src_cont = make_array_container_name (arr_ptr);

    arr_ptr = make_array_pointer (arr_name, arr_depth);
    split_array_ptr (&arr_name, &arr_depth, arr_ptr);
    if (this_depth > prev_depth) {
	arr_depth = this_depth;
    }
    
    string dest_cont = make_array_container_name (arr_ptr);

    ByteBuffer val;

    host_read_blob (src_cont,
		    object_name_t (object_id (idx)),
		    val);

    // splice in the partial value
    if (len < 0) {
	len = val.len();
    }
    memcpy (val.data() + off, new_val.data(), len);

    // perhaps copy the blob
    if (this_depth > prev_depth) {
	host_copy_container (src_cont, dest_cont);
    }

    // and write it into the dest
    host_write_blob (dest_cont,
		     object_name_t (object_id (idx)),
		     val);
}
*/


void do_unwrap (const ByteBuffer& val)
    
{
    ByteBuffer temp;

    size_t len = val.len();
    if (len < 24)
	len = 23;		// make sure the next if triggers
    
    if (len % 8 != 0) {
	temp = realloc_buf (val, round_up (len, 8));
    }
    else {
	temp = val;
    }

    // add 16 because we're skipping the mac check in unwrap, which passes a
    // longer input to decrypt()
    ByteBuffer dec (g_symrap->unwraplen (temp.len()) + 16);
    
    
//     cerr << "unwrappign buf of len " << temp.len()
// 	 << " into buf of len " << dec.len() << endl;
    
    g_symrap->unwrap (temp, dec);
}


