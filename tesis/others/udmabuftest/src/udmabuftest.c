#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <math.h>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#include "dma_uio.h"

#define V_SIZE (10000)//Cantidad de potenciales V
#define BLOCK_SIZE 8  //Tamaño del bloque nxn, donde n es igual a block_size
#if (V_SIZE%BLOCK_SIZE != 0)
#define ROW_DATA_SIZE ((BLOCK_SIZE*BLOCK_SIZE+BLOCK_SIZE)*(1+(V_SIZE/BLOCK_SIZE)))
#else
#define ROW_DATA_SIZE ((BLOCK_SIZE*BLOCK_SIZE+BLOCK_SIZE)*(V_SIZE/BLOCK_SIZE))
#endif

#define UNROLL 22


struct input{
	int length;
	int empty;
	float *data;
};

struct output_Hardware{
	int length;
	int empty;
	float *data;
};

struct output_Software{
	int length;
	int empty;
	float *data;
};


float V[V_SIZE];
float C[V_SIZE * V_SIZE];

void Simulate_SW(float *VectorC, float *VectorV, struct output_Software *out_sw);
void Simulate_HW(int size,int n_rows_size, unsigned int phy_addr_mm2s, unsigned int phy_addr_s2mm, struct input *input_stream, struct output_Hardware *out_hw, struct dev dma_uio, struct dev ip_uio);
void Generate_Data();
void Check_Results(struct output_Software *out_sw, struct output_Hardware *out_hw);

struct Buffer{
	int fd;
	void *memory_map;
	unsigned int phys_addr;
	unsigned int size;
	unsigned int direction;
	unsigned int sync_mode;
	unsigned int o_sync;
	unsigned int fd_sync_data;
};

int main(){
	printf("Hello\n");
	clock_t compute_acc = 0;
	clock_t init_compute = 0;
	//UIO
	//______________________________________________________________________________

	//DMA___________________________________________________________________________
	struct dev dma_uio;
	int dma_index=1;
	int dma_size=0x00010000;
	//init DMA uio
	printf("Initializing DMA UIO\n");
	dma_uio=init_uio(dma_index,dma_size);
	printf("Initialization completed\n\n");



	//Memory________________________________________________________________________
//	struct dev mem_uio;
//	int mem_index=3;
//	int mem_phy_addr=0x03e00000;
//	int mem_size=0x00100000;

	//Init mem uio
//	printf("Initializing Mem UIO\n");
//	mem_uio=init_uio(mem_index,mem_size);
//	printf("Initialization completed\n\n");

	struct Buffer buffer_mm2s;
	buffer_mm2s.size = (ROW_DATA_SIZE)*UNROLL*4;
	buffer_mm2s.sync_mode = 4;
	buffer_mm2s.direction=1;
	buffer_mm2s.o_sync=0;
	int            fd;
	char  attr[1024];

	if ((fd  = open("/sys/class/udmabuf/udmabuf0/phys_addr", O_RDONLY)) != -1) {
		read(fd, attr, 1024);
		sscanf(attr, "%x", &buffer_mm2s.phys_addr);
		close(fd);
	}

	if ((fd = open("/sys/class/udmabuf/udmabuf0/sync_mode", O_WRONLY)) != -1) {
		sprintf(attr, "%d", buffer_mm2s.sync_mode);
		write(fd, attr, strlen(attr));
		close(fd);
	}

	if ((fd = open("/sys/class/udmabuf/udmabuf0/sync_direction", O_WRONLY)) != -1) {
		sprintf(attr, "%d", buffer_mm2s.direction);
		write(fd, attr, strlen(attr));
		close(fd);
	}
	if ((buffer_mm2s.fd_sync_data = open("/sys/class/udmabuf/udmabuf0/sync_for_device", O_WRONLY)) != -1) {
	}

	printf("sync_mode=%d, O_SYNC=%d, ", buffer_mm2s.sync_mode, (buffer_mm2s.o_sync)?1:0);
	if ((buffer_mm2s.fd  = open("/dev/udmabuf0", O_RDWR | buffer_mm2s.o_sync)) != -1) {
		buffer_mm2s.memory_map = mmap(NULL, buffer_mm2s.size, PROT_READ|PROT_WRITE, MAP_SHARED, buffer_mm2s.fd, 0);
	}

	//s2mm

	struct Buffer buffer_s2mm;
	buffer_s2mm.size = (V_SIZE)*4;
	buffer_s2mm.sync_mode = 4;
	buffer_s2mm.direction=2;
	buffer_s2mm.o_sync=0;


	if ((fd  = open("/sys/class/udmabuf/udmabuf1/phys_addr", O_RDONLY)) != -1) {
		read(fd, attr, 1024);
		sscanf(attr, "%x", &buffer_s2mm.phys_addr);
		close(fd);
	}

	if ((fd = open("/sys/class/udmabuf/udmabuf1/sync_mode", O_WRONLY)) != -1) {
		sprintf(attr, "%d", buffer_s2mm.sync_mode);
		write(fd, attr, strlen(attr));
		close(fd);
	}

	if ((fd = open("/sys/class/udmabuf/udmabuf1/sync_direction", O_WRONLY)) != -1) {
		sprintf(attr, "%d", buffer_s2mm.direction);
		write(fd, attr, strlen(attr));
		close(fd);
	}
	if ((buffer_s2mm.fd_sync_data = open("/sys/class/udmabuf/udmabuf1/sync_for_cpu", O_WRONLY)) != -1) {
	}
	printf("sync_mode=%d, O_SYNC=%d, ", buffer_s2mm.sync_mode, (buffer_s2mm.o_sync)?1:0);
	if ((buffer_s2mm.fd  = open("/dev/udmabuf1", O_RDWR | buffer_s2mm.o_sync)) != -1) {
		buffer_s2mm.memory_map = mmap(NULL, buffer_s2mm.size, PROT_READ|PROT_WRITE, MAP_SHARED, buffer_s2mm.fd, 0);
	}




	//JavierIP________________________________________________________________________
	struct dev ip_uio;
	int ip_index=0;
	int ip_size=0x00010000;


	//Init ip uio
	printf("Initializing IP UIO\n");
	ip_uio=init_uio(ip_index,ip_size);
	printf("Initialization completed\n\n");
	volatile uint32_t *reg_ctrl_ip = (uint32_t *)((char *) ip_uio.memory_map + 0x00);
	volatile uint32_t *reg_vsize_ip = (uint32_t *)((char *) ip_uio.memory_map + 0x10);

	*reg_vsize_ip = V_SIZE;
	*reg_ctrl_ip = 0x01; //start IP
	printf("reg ctrl ip: 0x%x\n",*reg_ctrl_ip);
	//Test____________________________________________________________________________

	//Init variables____________________________________________________________________________
	printf("Init_test\n");
	printf("Variables_init_Gen_data\n");

	Generate_Data();
	//Set_mmap variables____________________________________________________________________________
	printf("Run_test\n");

	struct input *input_stream = (struct input *) malloc(sizeof(struct input));
	struct output_Hardware *out_hw = (struct output_Hardware *)malloc(sizeof(struct output_Hardware));
	struct output_Software *out_sw = (struct output_Software *) malloc(sizeof(struct output_Software));
	input_stream->data = (float *)buffer_mm2s.memory_map;
	out_hw->data = (float *)buffer_s2mm.memory_map;
	out_sw->data = (float *) malloc(sizeof(float)*V_SIZE);
	input_stream->length = ROW_DATA_SIZE*UNROLL;
	out_hw->length = V_SIZE;
	out_sw->length = V_SIZE;
	input_stream->empty = 0;
	out_hw->empty = 0;
	out_sw->empty = 0;


	int index_matrix_column = 0, index_matrix_row = 0, index_block = 0, row = 0,
			column = 0, row_temp = 0, count_column = 0, index_V = 0,
			index_C = 0, offset_C = 0;
	int blockNumbers = V_SIZE / BLOCK_SIZE; // Cantidad de bloques por fila
	if (V_SIZE % BLOCK_SIZE != 0) { // Si el bloque no es multiplo, se completa la matriz
		blockNumbers++;
	}
	int n_unrolled_blocks = blockNumbers / UNROLL;
	int r_unrolled_blocks = blockNumbers % UNROLL;
	//int size_V = blockNumbers * BLOCK_SIZE;
	int last_element = blockNumbers - 1;
	sprintf(attr, "%d", 1);
	unsigned int len=strlen(attr);

	printf("Start_test\n");

	fflush(stdout);
	init_compute=clock();
	for (index_matrix_row = 0; index_matrix_row < blockNumbers;
			index_matrix_row++) {   //Se recorre en columnas los bloques
		index_C = index_matrix_row * (BLOCK_SIZE * V_SIZE);
		index_V = 0;
		for (index_matrix_column = 0; index_matrix_column < blockNumbers;
				index_matrix_column++) { //Se recorre en columnas los bloques
			count_column = 0;

			// Here is filled one rows of blocks
			for (index_block = 0; index_block < BLOCK_SIZE; index_block++) { //Se divide los V, en bloques


				// Here is filled the voltages PREFIX for one block
				if (index_V < V_SIZE) {
					input_stream->data[input_stream->empty] = V[index_V];
					input_stream->empty++;
					//	printf("V %f \n", V[index_V]);
					count_column++;
				} else {
					input_stream->data[input_stream->empty] = 0;
					input_stream->empty++;
					//	printf("V %f \n", 0.0);
				}



				index_V++;
			}

			for (index_block = 0; index_block < BLOCK_SIZE; index_block++) { //Se divide los  C, en bloques

				if (index_matrix_column == last_element) { // Se realiza a lógica para determinar si se debe completar los V's segun los bloques
					column = BLOCK_SIZE - count_column;
					if (index_matrix_row != last_element) {
						row++;
						row_temp = row;
					}
				} else {
					column = 0;
				}
				if (index_matrix_row == last_element) {
					row++;
				}

				for (offset_C = index_C; offset_C < index_C + BLOCK_SIZE;
						offset_C++) {
					if (column < BLOCK_SIZE && row <= V_SIZE) {
						input_stream->data[input_stream->empty] = 0.04;//C[(index_block * V_SIZE + offset_C)];
						input_stream->empty++;
						//	printf("C %f \n", C[(index_block * V_SIZE + offset_C)]);
					} else {
						input_stream->data[input_stream->empty] = 0;
						input_stream->empty++;
						//	printf("C %f \n", 0.0);
					}
					column++;
				}

			}
			row = row_temp;
			index_C += BLOCK_SIZE;

			//Simulate_HW(input, output_Hardware, size_V); // por bloque
		}
//		printf("Index Row: %d\n",index_matrix_row);
		if(0<n_unrolled_blocks){
			if(index_matrix_row%UNROLL==(UNROLL-1) ){
				Simulate_HW(V_SIZE,4*BLOCK_SIZE*UNROLL, buffer_mm2s.phys_addr, buffer_s2mm.phys_addr ,input_stream, out_hw, dma_uio, ip_uio); //por fila
				n_unrolled_blocks-=1;
				input_stream->empty = 0;
			}
		}else{
			if(index_matrix_row%UNROLL==(r_unrolled_blocks-1)){
				Simulate_HW(V_SIZE,4*BLOCK_SIZE*r_unrolled_blocks, buffer_mm2s.phys_addr, buffer_s2mm.phys_addr ,input_stream, out_hw, dma_uio, ip_uio); //por fila
				input_stream->empty = 0;
				printf("n_unrolled_blocks: %d\n",index_matrix_row);
			}
		}
//		if(index_matrix_row%UNROLL==(UNROLL-1) ){
//			write(buffer_mm2s.fd_sync_data, attr, len);
//
//				Simulate_HW(V_SIZE,4*BLOCK_SIZE*UNROLL, buffer_mm2s.phys_addr, buffer_s2mm.phys_addr ,input_stream, out_hw, dma_uio, ip_uio); //por fila
//				n_unrolled_blocks-=1;
//
//
//		input_stream->empty = 0;
//		}
	}
	write(buffer_s2mm.fd_sync_data, attr, len);
	printf("End_HW\n");
	compute_acc+=(clock()-init_compute);

	float print_fp=((float)compute_acc)/CLOCKS_PER_SEC;
	printf("Time exec: %f\n",print_fp);
	compute_acc=0;
	init_compute=clock();
	Simulate_SW(C, V, out_sw);
	compute_acc+=clock()-init_compute;
	print_fp=((float)compute_acc)/CLOCKS_PER_SEC;
	printf("Time exec: %f\n",print_fp);
	printf("End_SW\n");

	Check_Results(out_sw, out_hw);
	printf("reg ctrl ip: 0x%x\n",*reg_ctrl_ip);
	printf("\nUnmapping and closing devices\n");
	free(out_sw->data);
	free(out_sw);
	free(out_hw);
	free(input_stream);
	munmap(dma_uio.memory_map,dma_size);
	munmap(buffer_mm2s.memory_map,buffer_mm2s.size);
	munmap(ip_uio.memory_map,ip_size);
	close(buffer_mm2s.fd_sync_data);
	close(buffer_s2mm.fd_sync_data);
	close(dma_uio.fd);
	close(buffer_mm2s.fd);
	close(ip_uio.fd);

	return 0;
}


void Simulate_SW(float *VectorC, float *VectorV, struct output_Software *out_sw) {

	int i = 0;
	int j = 0;
	int k = 0;

	float f = 0.0;
	float f_acc = 0.0;
	float v_acc = 0.0;
	float v = 0.0;
	float const hundred = -1.0 / 100.0;
	float I_c = 0.0;

	for (i = 0; i < V_SIZE; i++) {
		f_acc = 0;
		v_acc = 0;
		for (j = 0; j < V_SIZE; j++) {
			v = VectorV[i] - VectorV[j];
			f = v * expf(v * v * hundred);
			f_acc += f * VectorC[k];
			v_acc += v * VectorC[k];
			k++;
		}
		I_c = (0.8 * f_acc + 0.2 * v_acc);
		out_sw->data[out_sw->empty]=I_c;
		out_sw->empty++;
	}
}

void Generate_Data() {
	int i;
//	srand(5);
	for (i = 0; i < V_SIZE; i++) {
		V[i] = -81 ;//+ (float) (rand()) / ((float) (RAND_MAX / (-41 - (-81))));
	}
	for (int i = 0; i < V_SIZE * V_SIZE; i++) {
		C[i] = 0.04;
	}
}

void Check_Results(struct output_Software *out_sw, struct output_Hardware *out_hw) {
	int successful = 0;
	float sw_result, hw_result;
	float precision = 0.01;
//	while (!output_Software.empty()) {
	printf("lenght:%d\n",out_sw->length);
	for(int i=0; i<out_sw->length;i++){
		sw_result = out_sw->data[i];
		hw_result = out_hw->data[i];
		if (fabs(sw_result - hw_result) >= precision) {
			successful = 1;
			printf("Software Result %f \n", sw_result);
			printf("Hardware Result %f \n \n", hw_result);
			break;
		}
	}
	printf("******************************************\n");
	if (successful == 0) {
		printf("Successful result\n");
	} else {
		printf("Error\n");
	}
	printf("******************************************\n");
}


void Simulate_HW(int size,int n_rows_size, unsigned int phy_addr_mm2s,unsigned int phy_addr_s2mm, struct input *input_stream, struct output_Hardware *out_hw, struct dev dma_uio, struct dev ip_uio){

	volatile uint32_t *reg_ctrl_mm2s = (uint32_t *)((char *) dma_uio.memory_map + 0x00);
	volatile uint32_t *reg_ctrl_s2mm = (uint32_t *)((char *) dma_uio.memory_map + 0x30);
	volatile uint32_t *reg_status_mm2s = (uint32_t *)((char *) dma_uio.memory_map + 0x04);
	volatile uint32_t *reg_status_s2mm = (uint32_t *)((char *) dma_uio.memory_map + 0x34);
	volatile uint32_t *reg_addr_mm2s = (uint32_t *)((char *) dma_uio.memory_map + 0x18);
	volatile uint32_t *reg_addr_s2mm = (uint32_t *)((char *) dma_uio.memory_map + 0x48);
	volatile uint32_t *reg_size_mm2s = (uint32_t *)((char *) dma_uio.memory_map + 0x28);
	volatile uint32_t *reg_size_s2mm = (uint32_t *)((char *) dma_uio.memory_map + 0x58);

//Reset
	*reg_ctrl_mm2s = 0x100;
	*reg_ctrl_s2mm = 0x100;
	*reg_ctrl_mm2s = 0x00;
	*reg_ctrl_s2mm = 0x00;
//Clear interrupt
	int intinfo=1;
	write(dma_uio.fd, &intinfo, sizeof(int));

//Physical Address set
	*reg_addr_mm2s = (uint32_t)phy_addr_mm2s;
	*reg_addr_s2mm = (uint32_t)phy_addr_s2mm+(out_hw->empty)*4;
//	printf("mem empty: %d\n",out_hw->empty);
//	printf("mm2s addr: 0x%x\n",*reg_addr_mm2s);
//	printf("s2mm addr: 0x%x\n",*reg_addr_s2mm);

	volatile uint32_t *reg_ctrl_ip = (uint32_t *)((char *) ip_uio.memory_map + 0x00);
	volatile uint32_t *reg_vsize_ip = (uint32_t *)((char *) ip_uio.memory_map + 0x10);

	*reg_vsize_ip = V_SIZE;
	*reg_ctrl_ip = 0x01; //start IP



	*reg_ctrl_mm2s = (0x1<<12)|1;
	*reg_ctrl_s2mm = (0x1<<12)|1; //start stream

	*reg_size_mm2s = 4*input_stream->length;
	*reg_size_s2mm = n_rows_size;
//	printf("mm2s size: %d\n",*reg_size_mm2s);
//	printf("s2mm size: %d\n",*reg_size_s2mm);
//	printf("mm2s status: 0x%x\n",*reg_status_mm2s);
//	printf("s2mm status: 0x%x\n",*reg_status_s2mm);
//	printf("ip status: 0x%x\n",*reg_ctrl_ip);


	int icount;
	int err = read(dma_uio.fd, &icount, sizeof(int));
	if (err < 0) {
	  write(2, "An error occurred in the read.\n", 31);

	}
	else{
	  //perror("UIO read");
	}
	// Acknoledge interrupt
	*reg_status_mm2s=(0x1<<12)|0x10;
	*reg_status_s2mm=(0x1<<12)|0x10;
	out_hw->empty+=n_rows_size>>2;

//	for(int i=0;i<input_stream->length;i++){
//		printf("in: %f\n",input_stream->data[i]);
//	}
//	for(int i=0;i<out_hw->empty;i++){
//		printf("out: %f\n",out_hw->data[i]);
//	}




}



//void print_diff_time(struct timeval start_time, struct timeval end_time)
//{
//    struct timeval diff_time;
//    if (end_time.tv_usec < start_time.tv_usec) {
//        diff_time.tv_sec  = end_time.tv_sec  - start_time.tv_sec  - 1;
//        diff_time.tv_usec = end_time.tv_usec - start_time.tv_usec + 1000*1000;
//    } else {
//        diff_time.tv_sec  = end_time.tv_sec  - start_time.tv_sec ;
//        diff_time.tv_usec = end_time.tv_usec - start_time.tv_usec;
//    }
//    printf("time = %ld.%06ld sec\n", diff_time.tv_sec, diff_time.tv_usec);
//}
//
//int check_buf(unsigned char* buf, unsigned int size)
//{
//    int m = 256;
//    int n = 10;
//    int i, k;
//    int error_count = 0;
//    while(--n > 0) {
//      for(i = 0; i < size; i = i + m) {
//        m = (i+256 < size) ? 256 : (size-i);
//        for(k = 0; k < m; k++) {
//          buf[i+k] = (k & 0xFF);
//        }
//        for(k = 0; k < m; k++) {
//          if (buf[i+k] != (k & 0xFF)) {
//            error_count++;
//          }
//        }
//      }
//    }
//    return error_count;
//}
//
//int clear_buf(unsigned char* buf, unsigned int size)
//{
//    int n = 100;
//    int error_count = 0;
//    while(--n > 0) {
//      memset((void*)buf, 0, size);
//    }
//    return error_count;
//}
//
//void check_buf_test(unsigned int size, unsigned int sync_mode, int o_sync)
//{
//    int            fd;
//    unsigned char  attr[1024];
//    struct timeval start_time, end_time;
//    int            error_count;
//    unsigned char* buf;
//
//    if ((fd  = open("/sys/class/udmabuf/udmabuf0/sync_mode", O_WRONLY)) != -1) {
//      sprintf(attr, "%d", sync_mode);
//      write(fd, attr, strlen(attr));
//      close(fd);
//    }
//
//    printf("sync_mode=%d, O_SYNC=%d, ", sync_mode, (o_sync)?1:0);
//
//    if ((fd  = open("/dev/udmabuf0", O_RDWR | o_sync)) != -1) {
//      buf = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
//      gettimeofday(&start_time, NULL);
//      error_count = check_buf(buf, size);
//      gettimeofday(&end_time  , NULL);
//      print_diff_time(start_time, end_time);
//      close(fd);
//    }
//}
//
//void clear_buf_test(unsigned int size, unsigned int sync_mode, int o_sync)
//{
//    int            fd;
//    unsigned char  attr[1024];
//    struct timeval start_time, end_time;
//    int            error_count;
//    unsigned char* buf;
//
//    if ((fd  = open("/sys/class/udmabuf/udmabuf0/sync_mode", O_WRONLY)) != -1) {
//      sprintf(attr, "%d", sync_mode);
//      write(fd, attr, strlen(attr));
//      close(fd);
//    }
//
//    printf("sync_mode=%d, O_SYNC=%d, ", sync_mode, (o_sync)?1:0);
//
//    if ((fd  = open("/dev/udmabuf0", O_RDWR | o_sync)) != -1) {
//      buf = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
//      gettimeofday(&start_time, NULL);
//      error_count = clear_buf(buf, size);
//      gettimeofday(&end_time  , NULL);
//      print_diff_time(start_time, end_time);
//      close(fd);
//    }
//}


//struct Buffer {
//	int fd;
//	void* buf;
//	unsigned int buf_size;
//	unsigned int phys_addr;
//	unsigned int sync_mode;
//};
//
//void main()
//{
//	struct Buffer buffer;
//    int            fd;
//    unsigned char  attr[1024];
//    unsigned char* buf;
//    unsigned int   buf_size;
//    unsigned long  phys_addr;
//    unsigned long  debug_vma = 0;
//    unsigned long  sync_mode = 2;
//    int            error_count;
//    struct timeval start_time, end_time;
//
//    if ((fd  = open("/sys/class/udmabuf/udmabuf0/phys_addr", O_RDONLY)) != -1) {
//      read(fd, attr, 1024);
//      sscanf(attr, "%x", &phys_addr);
//      close(fd);
//    }
//
//    if ((fd  = open("/sys/class/udmabuf/udmabuf0/size"     , O_RDONLY)) != -1) {
//      read(fd, attr, 1024);
//      sscanf(attr, "%d", &buf_size);
//      close(fd);
//    }
//
//    if ((fd  = open("/sys/class/udmabuf/udmabuf0/sync_mode", O_WRONLY)) != -1) {
//      sprintf(attr, "%d", sync_mode);
//      write(fd, attr, strlen(attr));
//      close(fd);
//    }
//
//    if ((fd  = open("/sys/class/udmabuf/udmabuf0/debug_vma", O_WRONLY)) != -1) {
//      sprintf(attr, "%d", debug_vma);
//      write(fd, attr, strlen(attr));
//      close(fd);
//    }
//
//    printf("phys_addr=0x%x\n", phys_addr);
//    printf("size=%d\n", buf_size);
//
//    if ((fd  = open("/dev/udmabuf0", O_RDWR)) != -1) {
//      long last_pos = lseek(fd, 0, 2);
//      if (last_pos == -1) {
//        printf("lseek error\n");
//        exit(-1);
//      }
//      close(fd);
//    }
//
//
//}
