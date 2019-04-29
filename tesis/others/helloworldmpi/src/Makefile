CC=gcc 
CFLAGS+=\
	-pthread\
	-Wl,-rpath\
	-Wl,/home/marco/.openmpi/lib\
	-Wl,--enable-new-dtags\
	-L/home/marco/.openmpi/lib\
	-L/home/marco/.openmpi/lib\
	-lmpi\
	-I/home/marco/.openmpi/include\
all:	helloworldmpi
helloworldmpi:	helloworldmpi.o
	$(CC)	-o helloworldmpi helloworldmpi.o $(CFLAGS)
