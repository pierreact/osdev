# Isurus userland applications
# libc.a is built as part of the kernel phase (see src/Makefile);
# this Makefile only builds the apps themselves. Apps link against
# ../libc/libc.a which is already produced by phase 1.

all: apps

apps:
	$(MAKE) -C demo_app
	$(MAKE) -C dpdk_l2
	$(MAKE) -C dpdk_l3

clean:
	$(MAKE) -C demo_app clean
	$(MAKE) -C dpdk_l2 clean
	$(MAKE) -C dpdk_l3 clean

.PHONY: all apps clean
