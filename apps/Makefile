# Isurus userland applications
# Build libc first, then all apps.

all: libc apps

libc:
	$(MAKE) -C libc

apps: libc
	$(MAKE) -C demo_app

clean:
	$(MAKE) -C libc clean
	$(MAKE) -C demo_app clean

.PHONY: all libc apps clean
