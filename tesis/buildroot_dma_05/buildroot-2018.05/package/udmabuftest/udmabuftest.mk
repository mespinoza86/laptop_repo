UDMABUFTEST_VERSION = 1.0.0
UDMABUFTEST_SITE = $(TOPDIR)/package/udmabuftest/src
UDMABUFTEST_SITE_METHOD = local

#define UDMABUFTEST_BUILD_CMDS
#    $(MAKE) CC="$(TARGET_CC)" LD="$(TARGET_LD)" -C $(@D)
#endef

#define UDMABUFTEST_INSTALL_TARGET_CMDS
#    $(INSTALL) -D -m 0755 $(@D)/udmabuftest $(TARGET_DIR)/usr/bin/udmabuftest
#endef

CC=arm-linux-gnueabihf-gcc
UDMABUFTEST_LIB = $(TOPDIR)/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/
HOST_MPI_LIB = $(TOPDIR)/output/host/arm-buildroot-linux-uclibcgnueabihf/sysroot/usr/

TARGET_CPPFLAGS += -mcpu=cortex-a9 -mfpu=neon -std=c99 -ftree-vectorize -O3 -pthread -Wl,-rpath -Wl,$(HOST_MPI_LIB)/lib/ -Wl,--enable-new-dtags -L$(HOST_MPI_LIB)/lib/ -L$(HOST_MPI_LIB)/lib/ -lmpi -I$(UDMABUFTEST_LIB)/include -I$(HOST_MPI_LIB)/lib/
 


define UDMABUFTEST_BUILD_CMDS
    $(MAKE) $(TARGET_CONFIGURE_OPTS) CPU_TARGET=arm OS_TARGET=linux  -C $(@D)
endef
define UDMABUFTEST_INSTALL_TARGET_CMDS
    $(INSTALL) -D -m 0755 $(@D)/udmabuftest $(TARGET_DIR)/usr/bin/udmabuftest
endef

$(eval $(generic-package))
