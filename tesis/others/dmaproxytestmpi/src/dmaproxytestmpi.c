/* DMA Proxy Test Application
 *
 * Esta aplicacion contiene una aplicacion que simula redes neuronales en multiples FPGA usando MPI
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <string.h>
#include "ionlib.h"
#include "axitime.h"
#include <mpi.h>
#include <assert.h>
#include <time.h>

#include<pthread.h>



/*
	PARAMETROS DE CONFIGURACION
*/

#define MUX_FACTOR 2000
#define NSIZE MUX_FACTOR*3

#define STEPS 4
#define DMASIZE_IN NSIZE*4
#define DMASIZE_OUT 2*MUX_FACTOR*4

#define HLS_DIR "/dev/uio0"
#define TIMER_DIR "/dev/uio1"

// DEFINIR LA CANTIDAD DE ÍNDICES DEL ARREGLO
#define TEST_SIZE DMASIZE_IN

// DEFINIR LOS PUERTOS DEL USERSPACE AL DRIVER (CHAR DEVICE): ambos usan el DRIVER NAME definido en el driver como "dma_proxy"
#define TX_PORT "/dev/dma_proxy_tx"
#define RX_PORT "/dev/dma_proxy_rx"
MPI_Status Stat;
struct dma_proxy_channel_interface {
	unsigned char buffer[TEST_SIZE];
	enum proxy_status { PROXY_NO_ERROR = 0, PROXY_BUSY = 1, PROXY_TIMEOUT = 2, PROXY_ERROR = 3 } status;
	unsigned int length;
};

static struct dma_proxy_channel_interface *tx_proxy_interface_p;
static int tx_proxy_fd;

void *tx_thread(int *dmasize_in)
{
	int dummy, i;

	/* Set up the length for the DMA transfer and initialize the transmit
 	 * buffer to a known pattern.
 	 */
	tx_proxy_interface_p->length = *dmasize_in;

	/* Perform the DMA transfer and the check the status after it completes
 	 * as the call blocks til the transfer is done.
 	 */
	ioctl(tx_proxy_fd, 0, &dummy);

	if (tx_proxy_interface_p->status != PROXY_NO_ERROR)
		printf("Proxy tx transfer error\n");
}

struct Dev Axi_Timer;
struct Dev Axi_Hls_network;

int init_buffer[MUX_FACTOR*NUM_STATE_VAR];
float *FinalVaxonArray = NULL;
pthread_t tid2;
int *flag;
int muxfactor;
int contthread;
int boards;

/*****************************************************************************write_file*/

void writeLine(){
	FILE *fp;
	int offsetStep;
	int offset;
	float value;
	fp = fopen( "dataG.txt" , "a" );
	for(int j=0;j<contthread;j++){ 
		offsetStep=muxfactor*j;
		for(int a=0;a<(boards);a++){
			offset = offsetStep + ((muxfactor*contthread)*a);
				for(int b=0;b<muxfactor;b++){
					value=FinalVaxonArray[b+offset];
					fprintf(fp,"%f\t",value);
					//printf("guardar %f\n",value);
				}
			}
  		fputs("\n",fp);

	}
	fclose(fp);
}

/*****************************************************************************write_file*/

void* doSomeThing(void *arg)
{
	
	while(1){
		//printf("flag:%d sentence in thread: \n",*flag);
		if(*flag != 0){
			printf("Save data \n");
			writeLine();
			*flag = 0;
		}
	}

    return NULL;
}

/*****************************************************************************write_file*/

/*****************************************************************************READ_FILE*/
float* readFile(char *file,int chars,int lines){
	int i=0;
	int a = 0;
	char ch[chars];
	float *arr = (float *)malloc(lines * sizeof(float) * chars);
	FILE *myfile;
	myfile = fopen(file,"r");//"/home/rafa/Escritorio/trusted_IPs.txt"
	if(myfile== NULL){
		printf("can not open file \n");
		return NULL;
	}
	//printf("reading: %s\n",file);
	while(!feof(myfile)){

//		printf("chars %d, a %d \n",chars,a);
		if(a<=chars){
			fscanf(myfile,"%s",ch);
			if(a!=0){
				arr[i] = atof(ch);
//				printf("valor %f %d %d\n",arr[i],a,i);
				i+=1;
				}
			a+=1;}
		else{a=0;}
		//arr[i] = (char *) malloc(chars * sizeof(char));
		//strcpy(arr[i], ch);
		//i++;
	}   
	fclose(myfile);
	return arr;
}

/*****************************************************************************READ_FILE*/


float test_state_array[] = {
/*dend.V_dend*/		-60.0,
/*dend.Calcium_r*/	0.0112788,
/*dend.Potassium_s*/	0.0049291,
/*dend.Hcurrent_q*/	0.0337836,
/*dend.Ca2Plus*/	3.7152,
/*dend.I_CaH*/		0.5,
/*soma.g_CaL*/		0.68,
/*soma.V_soma*/		-60.0,
/*soma.Sodium_m*/	1.0127807,
/*soma.Sodium_h*/	0.3596066,
/*soma.Potassium_n*/	0.2369847,
/*soma.Potassium_p*/	0.2369847,
/*soma.Potassium_x_s*/	0.1,
/*soma.Calcium_k*/	0.7423159,
/*soma.Calcium_l*/	0.0321349,
/*axon.V_axon*/		-60.0,
/*axon.Sodium_m_a*/	0.003596066,
/*axon.Sodium_h_a*/	0.9,
/*axon.Potassium_x_a*/ 	0.2369847
};

int main(int argc, char *argv[])
{

	struct dma_proxy_channel_interface *rx_proxy_interface_p;
	int rx_proxy_fd;



	int dummy;
	pthread_t tid;
	int i,j;
	int err,tmp;
	float tmpf,time;
	uint32_t count0,count1,res,calbr;
	int c,rc,dest,dataWaitingFlag;
	const int MASTER =0;
	const int TAG_GENERAL=1;
	float sum_promedio = 0;
	float min = 100000;
	float max = 0;
	int dmasize_in = 0;
	int dmasize_out = 0;

 	double t1, t2,secs, time_comm,time_comp,timetmp;

if (argc != 3) {
   	fprintf(stderr, "Usage: /path/to/config /path/to/data.txt\n");
    	exit(1);
  	}

	char *configPath = argv[1];// path/to/config
	char *dataPath = argv[2]; // path/to/data.txt
	int num_configArray = 6; // amount of config variables
	float *configArray = (float *)malloc(sizeof(float) * num_configArray); //variables from config file
	
	int world_rank, world_size, color;
   	MPI_Init(&argc, &argv);
     	MPI_Comm myFirstComm;
	MPI_Comm_rank(MPI_COMM_WORLD, &world_rank);
	MPI_Comm_size(MPI_COMM_WORLD, &world_size);


	/************************************ init constants *************************************************/


if (world_rank == 0) {

	
	configArray = readFile(configPath,1,100);

	for(int dest=1;dest<world_size;dest++){ //broadcast configArray

		 for(int i=0;i<num_configArray;i++){ //send each variable to "dest" node
		    rc = MPI_Send(&configArray[i], 1, MPI_FLOAT, dest, TAG_GENERAL, MPI_COMM_WORLD);
			//printf("master send value: %f to node: %d\n",configArray[i],dest);
		 }
	}
  }

else{ //slaves nodes

	do { // Wait until a message is there to be received
      MPI_Iprobe(MASTER, 1, MPI_COMM_WORLD, &dataWaitingFlag, MPI_STATUS_IGNORE);
    } while (!dataWaitingFlag);

    // Get the message and put it in 'inMsg'
//printf("process %d receiving message\n",world_rank);
	float temp;
	 for(int i=0;i<num_configArray;i++){
	    rc = MPI_Recv(&temp, 1, MPI_FLOAT, MASTER, TAG_GENERAL, MPI_COMM_WORLD, &Stat);
	    configArray[i] = temp;
	 }

}

	/************************************ init constants *************************************************/

	MPI_Barrier(MPI_COMM_WORLD); /* checkpoint for all nodes ********************************************/

	contthread = 0;
	//boards = (int *)malloc(sizeof(int));
	//muxfactor = (int *)malloc(sizeof(int));
	float *dataArray = NULL;
 	float *data = NULL;
	int InitConsts = configArray[3];
	int nsize = configArray[2];
	int steps = configArray[1];
	boards = configArray[0];
	muxfactor = (int)(nsize/(boards));
//	printf("muxf:%d boards:%d nsize:%d\n",muxfactor,boards,nsize);
/*****************************************************************************************************
/**************************************************************************************************MPI_GROUPS
*/
if(world_rank==0){color = 0;}
else{color = 1;}

	MPI_Comm row_comm;
	MPI_Comm_split(MPI_COMM_WORLD, color, world_rank, &row_comm);

	int row_rank, row_size;
	MPI_Comm_rank(row_comm, &row_rank);
	MPI_Comm_size(row_comm, &row_size);
     	if (color == 0) {                     /* Group 0 communicates with group 1. */ 
      		MPI_Intercomm_create(row_comm, 0, MPI_COMM_WORLD, 1, 1, &myFirstComm); 
     	} 
	else if (color == 1) {                     /* Group 0 communicates with group 1. */ 
      		MPI_Intercomm_create(row_comm, 0, MPI_COMM_WORLD, 0, 1, &myFirstComm); 
     	} 
//printf("world_rank:%d\n",world_rank);
/**************************************************************************************************MPI_GROUPS
/**************************************************************************************************INIT_Timer_DMA
*/	
if(color==1){
	//if((nsize<muxfactor)||(nsize%muxfactor!=0)){
	//	return printf("Not valid input\n");
	//}
	//printf("row_rank:%d\n",row_rank);
	dmasize_in = nsize*sizeof(float);
	dmasize_out = 2*muxfactor*sizeof(float);

	//printf("n:%d m:%d s:%d\n",nsize,muxfactor,steps);

	strcpy(Axi_Timer.uiod, TIMER_DIR);
	strcpy(Axi_Hls_network.uiod, HLS_DIR);

	Axi_Timer.fd = open (Axi_Timer.uiod, O_RDWR);	//timer init
	if(Axi_Timer.fd < 1){
		perror(argv[0]);
		printf("Invalid UIO device file: %s.\n",Axi_Timer.uiod);
		return -1;
	}
	Axi_Hls_network.fd = open (Axi_Hls_network.uiod, O_RDWR);	//timer init
	if(Axi_Hls_network.fd < 1){
		perror(argv[0]);
		printf("Invalid UIO device file: %s.\n",Axi_Hls_network.uiod);
		return -1;
	}



	Axi_Timer.ptr = mmap(NULL, XTC_TIMER_MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, Axi_Timer.fd, 0);
	if(Axi_Timer.ptr == NULL){
		perror(argv[0]);
		printf("Invalid UIO mmap: %s.\n",Axi_Timer.uiod);
		return -1;
	}
	Axi_Hls_network.ptr = mmap(NULL, XHLS_ACCEL_MAP_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, Axi_Hls_network.fd, 0);
	if(Axi_Hls_network.ptr == NULL){
		perror(argv[0]);
		printf("Invalid UIO mmap: %s.\n",Axi_Hls_network.uiod);
		return -1;
	}

	init_timer(Axi_Timer.ptr);
	set_option(Axi_Timer.ptr);
	rst_timer(Axi_Timer.ptr);
	count0=get_timer(Axi_Timer.ptr);
	count1=get_timer(Axi_Timer.ptr);
	calbr=count1-count0;

	tx_proxy_fd = open(TX_PORT, O_RDWR);

	if (tx_proxy_fd < 1) {
		printf("Unable to open DMA proxy device file");
		return -1;
	}

	rx_proxy_fd = open(RX_PORT, O_RDWR);
	if (tx_proxy_fd < 1) {
		printf("Unable to open DMA proxy device file");
		return -1;
	}

	/* Step 2, map the transmit and receive channels memory into user space so it's accessible
 	 */
	tx_proxy_interface_p = (struct dma_proxy_channel_interface *)mmap(NULL, sizeof(struct dma_proxy_channel_interface),
									PROT_READ | PROT_WRITE, MAP_SHARED, tx_proxy_fd, 0);

	rx_proxy_interface_p = (struct dma_proxy_channel_interface *)mmap(NULL, sizeof(struct dma_proxy_channel_interface),
									PROT_READ | PROT_WRITE, MAP_SHARED, rx_proxy_fd, 0);

    	if ((rx_proxy_interface_p == MAP_FAILED) || (tx_proxy_interface_p == MAP_FAILED)) {
        	printf("Failed to mmap\n");
        	return -1;
    	}
}

/**************************************************************************************************INIT_Timer_DMA
*/
/***************************************************************************************************INIT_NET
*/
if(color == 0){//master 
//	data = readFile(dataPath,nsize,(steps+InitConsts)); //read data.txt


	dataArray = (float *)malloc(sizeof(float) * InitConsts * nsize); //variables from config file
	
//	for(int a=0;a<(boards);a++){//data params for each neuron
//		for(int j=0;j<muxfactor;j++){//each param
//			for(int i=0;i<InitConsts;i++){//each param
//				dataArray[i+(j*InitConsts)+(a*muxfactor*InitConsts)] = data[(a*muxfactor)+j+(i*nsize)];
//		}
//	}}

	for(int a=0;a<(boards);a++){//data params for each neuron
		for(int j=0;j<muxfactor;j++){
			for(int i=0;i<InitConsts;i++){
				dataArray[i+(j*InitConsts)+(a*muxfactor*InitConsts)] = test_state_array[i%InitConsts];
	}}}
//	for(int i=0;i<((Steps));i++){
//	for(int j=0;j<(nsize*InitConsts);j++){
//		printf("DataA: %f,data %f \n",dataArray[j],data[j]);}

}

	int lenInit=InitConsts*(nsize/(boards));
	float *sub_dataArray = (float *)malloc(sizeof(float) *lenInit);

	if (color == 0) {MPI_Scatter(dataArray,lenInit, MPI_FLOAT, sub_dataArray,lenInit, MPI_FLOAT,MPI_ROOT, myFirstComm);}
	else if (color == 1) {MPI_Scatter(dataArray,lenInit, MPI_FLOAT, sub_dataArray,lenInit, MPI_FLOAT, 0, myFirstComm);}


//	if(color == 1){
//		for(int w=0;w<(lenInit);w++){
//			printf("init para %d: #:%d %f\n",row_rank,w,sub_dataArray[w]);}}

float *VdenArray = (float *)malloc(sizeof(float) * (nsize-muxfactor));
if(color==1){
	for(int i=0;i<(nsize-muxfactor);i++){
		VdenArray[i]=sub_dataArray[i];	
	}

	err = set_network(Axi_Hls_network.ptr,nsize,muxfactor, (uint32_t *)sub_dataArray);
	if(err<0){
		return -1;	
	}

}


MPI_Barrier(MPI_COMM_WORLD); //checkpoint for all nodes *********************************************

/***************************************************************************************************
*/
free(configArray);
free(dataArray);
free(sub_dataArray);
free(data);

/***************************************************************************************************INIT_NET
*/
float *stepArray = NULL;
if(color == 0){//master
	stepArray = (float *)malloc(sizeof(float) * nsize);
}
float *VaxonArray=(int *)rx_proxy_interface_p->buffer;
float *lastStepArray = (float *)malloc(sizeof(float) * nsize);	
int cont=0;
float *sub_stepArray = (float *)malloc(sizeof(float) * muxfactor);
float *sub_VdenArray = (float *)malloc(sizeof(float) * muxfactor);
float *sub_VaxonArray = (float *)malloc(sizeof(float) *steps* muxfactor);
FinalVaxonArray = (float *)malloc(sizeof(float) *steps* nsize);
float *iAppArray = (int *)tx_proxy_interface_p->buffer;

double prom=0;
double maxp=0;
double minp=10000;
flag = (int *)malloc(sizeof(int));
*flag = 0;
     	if(color == 0){
		FILE *fp;
		fp = fopen( "dataG.txt" , "w" );
		fclose(fp);
    	 	int err;
		err = pthread_create(&tid2, NULL, &doSomeThing,NULL);
		if (err != 0)
		     printf("\ncan't create thread :[%s]", strerror(err));
		else
     			printf("\n Thread created successfully\n");
		printf("Init sim\n");
	} 

/**************************************************************************************************STEPS_GEN
*/


for(i=0;i<steps;i++){
	//time_comm=0.0;
	if(color == 0){//master 
		for(int j=0;j<nsize;j++){
                	if(i>20000-1 && i<25000-1){
                        	tmpf = 6.0;                             
                        }else{
                                tmpf = 0.0;
                        }stepArray[j] =tmpf;

			//stepArray[j] = 0.0;
		}
	
	}


        if(color == 0){
    	 	t1 = MPI_Wtime();
		
	} 
	if (color == 0) {MPI_Scatter(stepArray,muxfactor, MPI_FLOAT, sub_stepArray,muxfactor, MPI_FLOAT,MPI_ROOT, myFirstComm);}
	else if (color == 1) {MPI_Scatter(stepArray,muxfactor, MPI_FLOAT, sub_stepArray,muxfactor, MPI_FLOAT, 0, myFirstComm);}
        MPI_Barrier(MPI_COMM_WORLD); //checkpoint for all nodes ********************************************

        if(color == 0){
    	 	t2 = MPI_Wtime();
		time_comm+=(t2-t1)*1e3;
		timetmp = MPI_Wtime();
	} 

	if (color == 1){
		if(i!=0){
		int offset=0;
		for(int j=0;j<row_size;j++){
			if(offset==row_rank){offset+=1;} 
			else{
				for(int a=0;a<muxfactor;a++){
					VdenArray[a] = lastStepArray[a+(muxfactor*offset)]; // solo para almacenar algo distintivo de cada proceso
				}
			offset+=1;}}
		}

		for(int a=0;a<nsize;a++){
			//printf("vdend:%f iApp:%f\n",VdenArray[a],iAppArray[a]);
			if(a<muxfactor){iAppArray[a]=sub_stepArray[a];}
			else{iAppArray[a]=VdenArray[a-muxfactor];}
			
		}
/*
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_V_DEND_BASE)+0*sizeof(int),sizeof(float));
		printf("vdend %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_CALCIUM_R_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_POTASSIUM_S_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_HCURRENT_Q_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_CA2PLUS_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_DEND_I_CAH_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_G_CAL_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_V_SOMA_BASE)+0,sizeof(float));
		printf("vsoma %d: %f\n", i,tmpf);
		//memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_TEST_BASE)+0,sizeof(float));
		//printf("vtest %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_SODIUM_M_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_SODIUM_H_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_POTASSIUM_N_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_POTASSIUM_P_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_POTASSIUM_X_S_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_CALCIUM_K_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_SOMA_CALCIUM_L_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_V_AXON_BASE)+0*sizeof(int),sizeof(float));
		printf("vaxon %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_SODIUM_M_A_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_SODIUM_H_A_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
		memcpy(&tmpf,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_LOCAL_STATE0_AXON_POTASSIUM_X_A_BASE)+i,sizeof(float));
		printf("Set %d: %f\n", i,tmpf);
*/
		start_hw(Axi_Hls_network.ptr);
		rst_timer(Axi_Timer.ptr);
		count0=get_timer(Axi_Timer.ptr);
//		pthread_create(&tid, NULL, tx_thread, &dmasize_in);
	//		sleep(1);
		tx_proxy_interface_p->length = dmasize_in;

		/* Perform the DMA transfer and the check the status after it completes
 		 * as the call blocks til the transfer is done.
 		 */
		ioctl(tx_proxy_fd, 0, &dummy);

		if (tx_proxy_interface_p->status != PROXY_NO_ERROR)
			printf("Proxy tx transfer error\n");



		rx_proxy_interface_p->length = dmasize_out;
		ioctl(rx_proxy_fd, 0, &dummy);

		if (rx_proxy_interface_p->status != PROXY_NO_ERROR){printf("Proxy rx transfer error\n");}

		tmp = 0;
		while(tmp==0){
			memcpy(&tmp,Axi_Hls_network.ptr+(XHLS_ACCEL_AXILITES_ADDR_AP_CTRL),sizeof(int));
			tmp = (tmp&0x2)>>1;
		}

		//printf("outn:%f\n",*(float *)rx_proxy_interface_p->buffer);


		count1=get_timer(Axi_Timer.ptr);
		res=(count1-count0) - calbr;
		time=res*10e-6;
		if(time<min) min = time;
		if(time>max) max = time;
		sum_promedio += time;

		for(int j=0;j<(2*muxfactor);j++){
			if(j<muxfactor){sub_VaxonArray[j+(cont*muxfactor)]=VaxonArray[j];}
			else{sub_VdenArray[j-muxfactor]=VaxonArray[j];}

}
		MPI_Allgather(sub_VdenArray,muxfactor, MPI_FLOAT,lastStepArray,muxfactor, MPI_FLOAT, row_comm);//share VdenArray with everyone		

	}
	MPI_Barrier(MPI_COMM_WORLD);
	if(color == 0){
		secs=MPI_Wtime();	
		time_comp+= (secs-timetmp)*1e3;}

	cont+=1;
	if((cont*nsize*sizeof(float))>=50e6){
	//	printf("guardar %d\n",world_rank);

		while(*(int*)(flag)==1){}
		if(color == 0){
    			t1 = MPI_Wtime();
			MPI_Gather(sub_VaxonArray,cont *muxfactor, MPI_FLOAT,FinalVaxonArray,cont *muxfactor, MPI_FLOAT,MPI_ROOT, myFirstComm);}
		else if (color == 1) {MPI_Gather(sub_VaxonArray,cont *muxfactor, MPI_FLOAT,FinalVaxonArray,cont *muxfactor, MPI_FLOAT, 0, myFirstComm);}
		MPI_Barrier(MPI_COMM_WORLD);
		contthread=cont;
        	if(color == 0){
    	 		t2 = MPI_Wtime();
			time_comm+=(t2-t1)*1e3;
			*flag = 1;
			//while(*(int*)(flag)==1){}
			}
		//MPI_Barrier(MPI_COMM_WORLD);
		cont=0;

	}

        //if(color == 0){
	//	printf("Process: %d parcial comu: %f comput: %f \n",color,time_comm,time_comp);
	//} 


}

MPI_Barrier(MPI_COMM_WORLD);
if(cont!=0){
	printf("Board:%d nsize: %d\tmuxfactor: %d\tmin: %f\tmax: %f\tavrg: %f\n",world_rank,nsize,muxfactor,min,max,sum_promedio/steps);
	contthread=cont;
	if (color == 0) {
		t1 = MPI_Wtime();
		MPI_Gather(sub_VaxonArray,cont * muxfactor, MPI_FLOAT,FinalVaxonArray,cont * muxfactor, MPI_FLOAT,MPI_ROOT, myFirstComm);}
	else if (color == 1) {
		MPI_Gather(sub_VaxonArray,cont * muxfactor, MPI_FLOAT,FinalVaxonArray,cont * muxfactor, MPI_FLOAT, 0, myFirstComm);}
	if(color == 0){//master 
    	 	t2 = MPI_Wtime();
		time_comm+=(t2-t1)*1e3;
		while(*(int*)(flag)==1){}
		*flag = 1;
		printf("Process: %d final comu: %f comput: %f %d \n",color,(time_comm)/steps,time_comp/steps,steps);
		while(*(int*)(flag)==1){}
	}
	}
close(Axi_Hls_network.fd);
MPI_Barrier(MPI_COMM_WORLD); //checkpoint for all nodes ********************************************


	// Clean up
	free(stepArray);
	MPI_Comm_free(&myFirstComm); 
	free(VdenArray);
	free(sub_stepArray);
	free(lastStepArray);
	free(FinalVaxonArray);	
	MPI_Barrier(MPI_COMM_WORLD);
  	MPI_Finalize();
	/*************************************************/

	/* Unmap the proxy channel interface memory and close the device files before leaving
	 */
	munmap(tx_proxy_interface_p, sizeof(struct dma_proxy_channel_interface));
	munmap(rx_proxy_interface_p, sizeof(struct dma_proxy_channel_interface));

	close(tx_proxy_fd);
	close(rx_proxy_fd);
	return 0;
}
