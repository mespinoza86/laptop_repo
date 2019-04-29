#ifndef STREAM
#define STREAM


#include <hls_stream.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "ap_int.h"

using namespace hls;

template<int D, int U, int TI, int TD>

struct ap_axis {
	float data;
	ap_uint<1> last;

};

typedef ap_axis<32, 1, 1, 1> data_t;

void Simulate_HW(stream<data_t> &input,stream<data_t> &output, int size);


#endif
