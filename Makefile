
CFLAGS=-g
LDFLAGS=

all: geoc geot libgeo.so

GEOC_SRCS=geodata_compiler.c array.c
GEO_SRCS=geodata.c

geoc:$(GEOC_SRCS)
	gcc $(CFLAGS) $^ -o $@

geot:$(GEO_SRCS)
	gcc -D_TOOLS_ $(CFLAGS) $^ -o $@

libgeo.so: $(GEO_SRCS)
	gcc $(CFLAGS) -Werror $^ -fPIC -shared -o $@
	
clean: 
	rm -f geoc geot libgeo.so
	
