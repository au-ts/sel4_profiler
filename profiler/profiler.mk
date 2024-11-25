PROFILER_DIR := $(dir $(lastword $(MAKEFILE_LIST)))

profiler.elf: profiler/profiler.o
	$(LD) $(LDFLAGS) $< $(LIBS) -o $@

profiler/profiler.o: ${PROFILER_DIR}/profiler.c
	mkdir -p profiler
	${CC} -c ${CFLAGS} -I ${PROFILER_DIR} -o $@ $<

-include profiler/profiler.d