################################################################################
#
# wireguard
#
################################################################################

DMAPROXY_VERSION = 1.0.0
DMAPROXY_SITE = $(TOPDIR)/package/dmaproxy/src
DMAPROXY_SITE_METHOD = local
DMAPROXY_LICENSE = GPL-2.0
DMAPROXY_LICENSE_FILES = COPYING
DMAPROXY_DEPENDENCIES = host-pkgconf libmnl


ifeq ($(BR2_LINUX_KERNEL),y)
DMAPROXY_MODULE_SUBDIRS = .
DMAPROXY_MODULE_MAKE_OPTS = KVERSION=$(LINUX_VERSION_PROBED)
$(eval $(kernel-module))
endif

$(eval $(generic-package))
