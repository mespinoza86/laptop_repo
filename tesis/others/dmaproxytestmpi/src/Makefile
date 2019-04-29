CC=gcc 
CFLAGS+=\
	-pthread\
	-Wl,-rpath\
	-Wl,/home/marco/.openmpi/lib\
	-Wl,--enable-new-dtags\
	-L/home/marco/.openmpi/lib\
	-L/home/marco/.openmpi/lib\
	-lmpi\
	-lpthread\
	-std=c99\
	-I/home/marco/.openmpi/include\
	-I.
all:	dmaproxytestmpi 
dmaproxytestmpi:	dmaproxytestmpi.o ionlib.o 
	$(CC)	-o dmaproxytestmpi dmaproxytestmpi.o ionlib.o $(CFLAGS)

