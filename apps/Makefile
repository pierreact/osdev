# Isurus userland applications
# Build libc first, then all apps.

all: libc apps

libc:
	$(MAKE) -C libc

apps: libc
	$(MAKE) -C demo_app
	$(MAKE) -C dpdk_l2

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C demo_app clean
	$(MAKE) -C dpdk_l2 clean

.PHONY: all libc apps clean
