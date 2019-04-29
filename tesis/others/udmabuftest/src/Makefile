APP = udmabuftest
CC=arm-linux-gnueabihf-gcc

# Add any other object files to this list below
APP_OBJS = udmabuftest.o

all: build

build: $(APP)

$(APP): $(APP_OBJS)
	$(CC) -mcpu=cortex-a9 -mfpu=neon -ftree-vectorize -O3 $(LDFLAGS) -o $@ $(APP_OBJS) $(LDLIBS)

clean:
