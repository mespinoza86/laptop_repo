HELLOWORLDMPI_VERSION = 1.0.0
HELLOWORLDMPI_SITE = $(TOPDIR)/package/helloworldmpi/src
HELLOWORLDMPI_SITE_METHOD = local
HELLOWORLDMPI_DEPENDENCIES = openmpi

CC=arm-linux-gnueabi-gcc
HELLOWORLDMPI_LIB = $(TOPDIR)/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/
HOST_MPI_LIB = $(TOPDIR)/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/

TARGET_CPPFLAGS += -pthread -Wl,-rpath -Wl,$(HOST_MPI_LIB)/lib/ -Wl,--enable-new-dtags -L$(HOST_MPI_LIB)/lib/ -L$(HOST_MPI_LIB)/lib/ -lmpi -I$(HELLOWORLDMPI_LIB)/include -I$(HOST_MPI_LIB)/lib/

define HELLOWORLDMPI_BUILD_CMDS
    $(MAKE) $(TARGET_CONFIGURE_OPTS) CPU_TARGET=arm OS_TARGET=linux  -C $(@D)
endef
define HELLOWORLDMPI_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/helloworldmpi $(TARGET_DIR)/usr/bin/helloworldmpi
endef

$(eval $(generic-package))
