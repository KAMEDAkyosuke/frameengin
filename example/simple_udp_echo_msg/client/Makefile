MAKE	   = make --print-directory
CC         = clang
CFLAGS    += --std=c99 -Wall -Wextra -Wno-unused-parameter -Wno-unused-function -pedantic
LDFLAGS   += 
INCLUDES  +=
LDLIBS    += 

TARGETS    = main

all: $(OBJS) $(TARGETS)

$(TARGETS): main.c $(OBJS)
	$(CC) -o main main.c ../../../src/frameengine.c $(CFLAGS) $(LDFLAGS) $(LDLIBS) $(ARGS)

.PHONY: clean
clean:
	$(RM) *.o $(TARGETS)
	@for dir in $(SUBDIRS) ; do \
		(cd $$dir && $(MAKE) clean) ; \
	done

.PHONY: check-syntax
check-syntax:
	$(CC) $(CFLAGS) -fsyntax-only $(CHK_SOURCES)

