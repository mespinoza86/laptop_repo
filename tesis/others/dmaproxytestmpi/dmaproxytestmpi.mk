DMAPROXYTESTMPI_VERSION = 1.0.0
DMAPROXYTESTMPI_SITE = $(TOPDIR)/package/dmaproxytestmpi/src
DMAPROXYTESTMPI_SITE_METHOD = local
DMAPROXYTESTMPI_DEPENDENCIES = openmpi

CC=arm-linux-gnueabi-gcc
DMAPROXYTESTMPI_LIB = $(TOPDIR)/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/
HOST_MPI_LIB = $(TOPDIR)/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/

TARGET_CPPFLAGS += -pthread -Wl,-rpath -Wl,$(HOST_MPI_LIB)/lib/ -Wl,--enable-new-dtags -L$(HOST_MPI_LIB)/lib/ -L$(HOST_MPI_LIB)/lib/ -lmpi -I$(DMAPROXYTESTMPI_LIB)/include -I$(HOST_MPI_LIB)/lib/

define DMAPROXYTESTMPI_BUILD_CMDS
    $(MAKE) $(TARGET_CONFIGURE_OPTS) CPU_TARGET=arm OS_TARGET=linux  -C $(@D)
endef
define DMAPROXYTESTMPI_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/dmaproxytestmpi $(TARGET_DIR)/usr/bin/dmaproxytestmpi
endef

$(eval $(generic-package))
