#include "Stream.h"

#define BLOCK_SIZE 8// nxn ejm n=4 => 4x4
//#define SIZE  16



void Simulate_HW(hls::stream<data_t> &input, hls::stream<data_t> &output,
		int size) {
	static float F_acc[BLOCK_SIZE];
	static float V_acc[BLOCK_SIZE];
	static float savedData[BLOCK_SIZE];
	static float nextSavedData[BLOCK_SIZE];
	static float processedData[BLOCK_SIZE];
	static int blockNumber = 0;
	static int vertical = BLOCK_SIZE;


#pragma HLS ARRAY_PARTITION variable=nextSavedData complete dim=1
#pragma HLS ARRAY_PARTITION variable=savedData complete dim=1
#pragma HLS DATAFLOW
#pragma HLS array_partition variable=F_acc complete
#pragma HLS array_partition variable=V_acc complete
#pragma HLS INTERFACE s_axilite port=return
#pragma HLS INTERFACE axis  port=input
#pragma HLS INTERFACE axis  port=output
#pragma HLS INTERFACE s_axilite register port=size

	int i, j;
	data_t readData, conductibity;

	ROW_LOOP: while (blockNumber < size) {
#pragma HLS LOOP_TRIPCOUNT max=625
//#pragma HLS DEPENDENCE
//#pragma HLS PIPELINE II=6
#pragma HLS PIPELINE II=300

		float Vi = 0.0, Vj = 0.0;
		float V = 0.0, F = 0.0;
		float f_acc = 0.0, v_acc = 0.0;
		float const hundred = -1.0 / 100.0;
		READ_V_LOOP: for (i = 0; i < BLOCK_SIZE; i++) {

			readData = input.read();
			processedData[i] = readData.data;
			if (blockNumber == 0 && vertical == BLOCK_SIZE) {
				savedData[i] = readData.data;
			} else if (blockNumber == vertical) {
				nextSavedData[i] = readData.data;
			}
		}

		CALCULATE_LOOP_1: for (i = 0; i < BLOCK_SIZE; i++) {
			f_acc = 0;
			v_acc = 0;
			CALCULATE_LOOP_2: for (j = 0; j < BLOCK_SIZE; j++) {
				Vi = savedData[i];
				Vj = processedData[j];
				V = Vi - Vj;
				F = V * expf(V * V * hundred);
				conductibity = input.read();
				f_acc += F * conductibity.data;
				v_acc += V * conductibity.data;
			}

			if (blockNumber != 0) {
				f_acc += F_acc[i];
				v_acc += V_acc[i];
			}
			F_acc[i] = f_acc;
			V_acc[i] = v_acc;
		}

		blockNumber += BLOCK_SIZE;
	}


	data_t I;
	I_LOOP: for (i = 0; i < BLOCK_SIZE; i++) {
#pragma HLS PIPELINE
		savedData[i] = nextSavedData[i];
		blockNumber = 0;
		vertical++;
		if (i == (BLOCK_SIZE - 1)) {
			I.last = 1;
		} else {
			I.last = 0;
		}
		I.data = 0.8 * F_acc[i] + 0.2 * V_acc[i];
		output.write(I);
	}

	if(vertical==size+8){
		blockNumber = 0;
		vertical = BLOCK_SIZE;
	}

}

