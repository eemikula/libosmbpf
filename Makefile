TARGETS=lib/libosmpbf.so lib/libosmpbf.a
CFLAGS=

all: $(TARGETS)

clean:
	@$(MAKE) clean -C src

lib/libosmpbf.so:
	@$(MAKE) -C src

lib/libosmpbf.a:
	@$(MAKE) -C src

