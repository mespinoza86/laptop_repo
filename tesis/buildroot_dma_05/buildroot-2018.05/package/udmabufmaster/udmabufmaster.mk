################################################################################
#
# udmabufmaster
#
################################################################################

UDMABUFMASTER_VERSION = 1.0.0
UDMABUFMASTER_SITE = $(TOPDIR)/package/udmabufmaster/src
UDMABUFMASTER_SITE_METHOD = local
UDMABUFMASTER_LICENSE = GPL-2.0
UDMABUFMASTER_LICENSE_FILES = COPYING
UDMABUFMASTER_DEPENDENCIES = host-pkgconf libmnl


ifeq ($(BR2_LINUX_KERNEL),y)
UDMABUFMASTER_MODULE_SUBDIRS = .
UDMABUFMASTER_MODULE_MAKE_OPTS = KVERSION=$(LINUX_VERSION_PROBED)
$(eval $(kernel-module))
endif

$(eval $(generic-package))
