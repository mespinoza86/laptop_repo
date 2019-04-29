#include "ionlib.h"


enum cell_struct_order{
	dend_vdend = 0,
	dend_calc,
	dend_pot,
	dend_hcurr,
	dend_ca2p,
	dend_icah,
	soma_gcal,
	soma_vsoma,
	soma_sodm,
	soma_sodh,
	soma_potn,
	soma_potp,
	soma_potxs,
	soma_calk,
	soma_call,
	axon_vaxon,
	axon_sodma,
	axon_sodha,
	axon_potxa
};

int start_hw(void *ptr){
	uint32_t tmp;
//interrupt Enable
	//tmp = HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_IER);
	//HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_IER) = tmp|0x01;
//Global interrupt Enable
	//HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_GIE) = 0x01;
//start hls
	//tmp = HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_AP_CTRL) & 0x80;
	//HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_AP_CTRL) =  tmp|0x01;
	

	//interrupt Enable
//	tmp = HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_IER);
//	HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_IER) = tmp|0x01;
	//Global interrupt Enable
//	HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_GIE) = 0x01;
	//start hls
//	tmp = HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_AP_CTRL) & 0x80;
//	HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_AP_CTRL) =  tmp|0x01;
//	tmp = HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_AP_CTRL) & 0x80;
//	HW_REGISTER(ptr,XHLS_ACCEL_AXILITES_ADDR_AP_CTRL) =  tmp|0x01;

	memcpy(&tmp,ptr+(XHLS_ACCEL_AXILITES_ADDR_AP_CTRL),sizeof(int));
	tmp = (tmp&0x80)|0x01;
	memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_AP_CTRL),&tmp,sizeof(int));
	memcpy(&tmp,ptr+(XHLS_ACCEL_AXILITES_ADDR_AP_CTRL),sizeof(int));
	tmp = (tmp&0x80)|0x01;
	memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_AP_CTRL),&tmp,sizeof(int));

	return 0;
}

int set_network(void *ptr, int n_size, int mux_factor, uint32_t *buffer){
	int i;
	float tmp;
	//HW_REGISTER(ptr, XHLS_ACCEL_AXILITES_ADDR_N_SIZE_DATA) = n_size;
	//HW_REGISTER(ptr, XHLS_ACCEL_AXILITES_ADDR_MUX_FACTOR_DATA) = mux_factor;
	memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_N_SIZE_DATA), &n_size, sizeof(int));
	memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_MUX_FACTOR_DATA), &mux_factor, sizeof(int));
	printf("Nsizein %d\n",*(uint32_t *)(ptr+(XHLS_ACCEL_AXILITES_ADDR_N_SIZE_DATA)));
	printf("muxin %d\n",*(uint32_t *)(ptr+(XHLS_ACCEL_AXILITES_ADDR_MUX_FACTOR_DATA)));
	if(mux_factor>n_size){
		printf("Mux Factor es mayor que n size\n");
		return -1;
	}

	for(i=0;i<mux_factor;i++){
//	for(i=0;i<1;i++){
		//printf("test %d\n",i);
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_V_DEND_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+dend_vdend],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_CALCIUM_R_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+dend_calc],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_POTASSIUM_S_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+dend_pot],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_HCURRENT_Q_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+dend_hcurr],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_CA2PLUS_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+dend_ca2p],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_I_CAH_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+dend_icah],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_G_CAL_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_gcal],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_V_SOMA_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_vsoma],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_SODIUM_M_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_sodm],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_SODIUM_H_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_sodh],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_POTASSIUM_N_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_potn],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_POTASSIUM_P_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_potp],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_POTASSIUM_X_S_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_potxs],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_CALCIUM_K_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_calk],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_CALCIUM_L_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+soma_call],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_V_AXON_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+axon_vaxon],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_SODIUM_M_A_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+axon_sodma],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_SODIUM_H_A_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+axon_sodha],sizeof(float));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_POTASSIUM_X_A_BASE)+i*sizeof(int), &buffer[(i*NUM_STATE_VAR)+axon_potxa],sizeof(float));

	}
	tmp = CONDUCTANCE;

	for(i=0;i<n_size;i++){
//		REG_WR(tmp,ptr,XHLS_ACCEL_AXILITES_ADDR_CONNECTIVITY_MATRIX_BASE+(i*sizeof(float)));
		memcpy(ptr+(XHLS_ACCEL_AXILITES_ADDR_CONNECTIVITY_MATRIX_BASE)+i*sizeof(int), &tmp,sizeof(float));
		
	}

	return 0;
}

int write_step(int fd, void *ptr, int dma_size_in, int dma_size_out, char *buffer){
	int dummy;
#ifdef DEBUG
	printf("Init write step, dma size: %d", dma_size_in, dma_size_out);
#endif

//	start_hw(ptr);
//	scanf("go: ",&dummy);
	ioctl(fd,IOCTL_SET_DMA_SIZE_IN, dma_size_in);
	ioctl(fd,IOCTL_SET_DMA_SIZE_OUT, dma_size_out);
//	printf("write %f, %f\n",((float *)buffer)[0],((float *)buffer)[101]);
	ioctl(fd,IOCTL_SET_STEP, buffer);
	ioctl(fd,IOCTL_PROCESS_STEP, &dummy);

	

	return 0;
}

int read_step(int fd, int dma_size_out, char *buffer){
#ifdef DEBUG
	printf("Init read step, dma size: %d", dma_size_out);
#endif
	read(fd,buffer,dma_size_out);
//	printf("read %f, %f\n",((float *)buffer)[0],((float *)buffer)[10]);	
	return 0;
}

