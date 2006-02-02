#include <iostream>
#include <iomanip>

#include <assert.h>


#include "batcher-network.h"



using std::cout;
using std::endl;
using std::setw;


int NwStaticInit::num_insts;


#ifdef _TESTING_BATCHER_NETWORK

void print_comp_indices (index_t a, index_t b) {
    cout << setw(4) << a << setw(4) << b << endl;
}



int main (int argc, char * argv[]) {

    assert (argc > 1);
    
    int N = atoi (argv[1]);

    run_batcher (N, print_comp_indices);

}

#endif // _TESTING_BATCHER_NETWORK
