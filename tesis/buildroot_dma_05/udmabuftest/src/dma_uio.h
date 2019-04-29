
//Structs_______________________________________________________________________
typedef struct RegisterAddressMap{
	uint32_t control;
	uint32_t status;
	uint64_t reserved1;
	uint32_t reserved2;
	uint32_t *addr_lo;
	uint32_t *addr_hi;
  uint32_t length;
}RegisterAddressMap;

typedef struct dev {
	int idx;
	int fd;
	char uiod[20];
	void *memory_map;
	char _phy_addr;
	int size;
} dev;

typedef struct axidma{
	dev dma_uio;
	RegisterAddressMap *mm2s;
	RegisterAddressMap *s2mm;
}axidma;





//UIO___________________________________________________________________________
int open_uio(dev device){   //Open  UIO
	device.fd = open (device.uiod, O_RDWR);
	return device.fd;
}


void* map_uio(int fd,int size){ //Makes UIO memory map
	void *memory_map;
	memory_map = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	return memory_map;
}


dev init_uio(int index, int size){ // Initialize a new uio
	dev device;

	device.idx=index;
	device.size=size;

	//Set Directory and uio
	char idx[10];
	char dir[20]="/dev/uio";
	//char dir[20]="./uio";
	sprintf(idx,"%d",device.idx);// Convert int index to char
	strcat(dir,idx);
	strcpy(device.uiod,dir);
	printf("UIO%d selected\n",device.idx);


	//Opening uio
	printf("Opening uio%d\n",device.idx);
	device.fd=open_uio(device);

	if(device.fd < 1){
 	 printf("Invalid UIO device file: %s.\n",device.uiod);
  }
	else{
 	 printf("UIO opened\n");
  }


	//mmap the UIO device
	printf("Mapping UIO\n");
	device.memory_map=map_uio(device.fd,device.size);
	if(device.memory_map==-1){
		printf("UIO not mapped\n");
	}
	else{
		printf("UIO mapped\n");
	}
	printf("Memory mapped direction: %p \n", device.memory_map);

	return device;

}


//DMA___________________________________________________________________________

void reset(RegisterAddressMap *,RegisterAddressMap*);

axidma dma_init(dev uio){
	axidma dma;

  printf("Assigning UIO\n");
	dma.dma_uio=uio;						//Assign the uio for the dma
	printf("Assigning mm2s\n");
	dma.mm2s=(RegisterAddressMap *)uio.memory_map;    //Assign the mm2s memory map
	printf("Assigning s2mm\n");
	dma.s2mm=(RegisterAddressMap *)(uio.memory_map+(0x30>>2)); //Assign the s2mm memory map 12


	if (halted(dma.s2mm,dma.mm2s) == 0){ //Reset the dma
		reset(dma.s2mm,dma.mm2s);
	}

	return dma;
}


void reset(RegisterAddressMap* mm2s,RegisterAddressMap* s2mm){
	mm2s->control = 4;
	s2mm->control = 4;
  mm2s->control = 0;
  s2mm->control = 0;
}

int dma_idle(RegisterAddressMap dma){
	return (dma.status&2) == 2;
}
int dma_halted(RegisterAddressMap dma){
	return ((dma.status&1) == 1);
}

int idle(RegisterAddressMap s2mm, RegisterAddressMap mm2s ){
	return (dma_idle(s2mm) & dma_idle(mm2s));
}
int halted(RegisterAddressMap s2mm, RegisterAddressMap mm2s ){
	return (dma_halted(s2mm) & dma_halted(mm2s));
}


int _launch_stream( RegisterAddressMap *function, uint32_t *buf, int irq_flags, int size){
		uint32_t* phy_addr= buf;

		function->addr_lo = phy_addr;
		function->control = (irq_flags<<12)|1;
		function->length = size;
		return 0;
}


int launch( axidma dma,uint32_t *src_buf, uint32_t *dst_buf, int size){
    int irq_flags = 0x1;
		unsigned icount=0;

		_launch_stream(dma.s2mm, src_buf, irq_flags,size);
		printf("mm2s completed \n");
		_launch_stream(dma.s2mm, dst_buf, irq_flags,size);
		printf("s2mm completed \n");


		return read(dma.dma_uio.fd, &icount, sizeof(int)); //Read the interruption
}
