#pragma once

#include "profiler.h"

/* ----- THE FOLLOWING DEFINES ARE FOR CONFIGURING THE PROFILER ------*/

// TODO: NEED TO HAVE A BETTER WAY OF INJECTING THE MAPPINGS FROM THE SYSTEM DESCRIPTION

/*
    This is the string that maintains the mapping between profiler id's and elf names.
    The profiler id is the value with which you make the the seL4_ProfilerRegisterThread(id) syscall.

    Please change this string when adding processes to profile. Use the following structure:

    "<elf_name>: <profiling_id>\n
    <elf_name>: <profiling_id"

    NOTE: Seperate pairs with new lines.
*/

#define MAPPINGS_STR "echo.elf: 52\ndummy_prog.elf: 1\ndummy_prog2.elf: 2"

#define CYCLE_COUNTER_PERIOD 1.3e6