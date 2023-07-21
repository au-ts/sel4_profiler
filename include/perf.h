#pragma once

#include <stdint.h>

/* Perf file format as described by CERN */

// Defines the max depth that we will record of the call stack 
#define MAX_INSN 16

struct perf_file_section {
    uint64_t offset; /* File offset of the section. */
    uint64_t size;
};

typedef struct perf_file_section perf_file_section_t;

struct perf_file_header {
    uint64_t magic;     /* Magic number, has to be "PERFFILE" */
    uint64_t size;      /* Size of this header*/
    uint64_t attr_size; /* Size of one attribute section, if it does not match, the entries may need to be swapped. */
    perf_file_section_t attrs;
    perf_file_section_t data;
    perf_file_section_t event_types;  /* This is apparently ignored, according to Linux source code */
    uint64_t p;
    uint64_t p1;
    uint64_t p2;
    uint64_t p3;
};

typedef struct perf_file_header perf_file_header_r;

struct perf_trace_event_type {
    uint64_t event_id;  /* The ID of the current event */
    char name[64];      /* Name of the event */
};

typedef struct perf_trace_event_type perf_trace_event_type_t;

struct perf_event_attr {
    uint32_t type;                  /* Major type (software/hardware/tracepoint/etc...)*/
    uint32_t size;                  /* Size of this structure */
    uint64_t config;                /* Link to the event id in the perf_trace_event_type struct */
    uint64_t sample_period;         /* Number of events occured before sample if .freq is not set */
    uint64_t sample_freq;           /* Frequency for sampling if .freq is set */
    uint64_t sample_type;           /* Information on what is stored in the sampling record */
    uint64_t read_format;           /* NOT SURE WHAT THIS DOES */
    uint64_t disabled:1,            /* off by default */
            inherit:1,              /* children inherit it */
            pinned:1,               /* must always be on PMU */
            exclusive:1,            /* only group on PMU */
            exclude_user:1,         /* don't count user*/
            exclude_kernel:1,       /* ditto kernel */
            exclude_hv:1,           /* ditto hypervisor */
            exclude_idle:1,         /* don't count when idle */
            mmap:1,                 /* MMAP records are included in file */
            comm:1,                 /* COMM records are included in file */
            freq:1,                 /* If set, sample freq is valid otherwise sample_period */
            inherit_stat:1,         /* Per task counts */
            enable_on_exec:1,       /* next exec enables */
            task:1,                 /* trace fork/exit */
            watermark:1,            /* wakeup_watermark */
            precise_ip:2,           /* “0 - SAMPLE IP can have arbitrary skid”
                                        “1 - SAMPLE IP must have constant skid”
                                        “2 - SAMPLE IP can have arbitrary skid”
                                        “3 - SAMPLE IP must have 0 skid */
            mmap_data:1,            /* non-exec mmap data */
            sample_id_all:1,        /* If set, the following records hold data. We assume this bit to be set */
            reserved:45;   
    union {
        uint32_t wakeup_events;         /* wakeup every n events */
        uint32_t wakeup_watermark;      /* Bytes before wakeup */
    };
    uint32_t bp_type;
    union {
        uint64_t bp_addr;
        uint64_t config1;
    };
    union {
        uint64_t bp_len;
        uint64_t config2;
    };
};

typedef struct perf_event_attr perf_event_attr_t;

struct perf_file_attr {
    perf_event_attr_t attr;      /* As described in the structure perf_event_attr */
    perf_file_section_t ids;     /* list of uint64_t identifier for matching with .id of the perf sample */
};

typedef struct perf_file_attr perf_file_attr_t;

/* --- THE FOLLOWING STRUCTS DESCRIBE THE DATA SECTION OF THE PERF FILE FORMAT --- */

/* TODO - ADD ENUMERATOR FOR THE PERF_EVENT_TYPE */


struct perf_event_header {
    uint32_t type;              /* Value from the enumator perf_event_type */
    uint16_t misc;
    uint16_t size;
};

typedef struct perf_event_header perf_event_header_t;

struct comm_event {
    uint32_t pid;               /* Process ID, in our case, the id of the protection domain */
    uint32_t tid;               /* This is redundant, same as pid in our case */
    char comm[16];              /* Name of the application */
};

typedef struct comm_event comm_event_t;

/* We will create one of these samples everytime we interrupt in the profiler */
struct perf_sample {
    uint64_t ip;                /* Instruction Pointer */
    uint32_t pid;               /* Process ID */
    uint32_t tid;               /* Thread ID -- Unused*/
    uint64_t time;              /* Timestamp */
    uint64_t addr;              /* For sampling memory maps/unmaps */
    uint64_t id;                /* Identification @kwinter not sure what this is supposed to identify */
    uint64_t stream_id;
    uint32_t cpu;               /* used CPU */
    uint32_t res;
    uint64_t period;            /* Number of events */
    uint64_t values;            /* @kwinter not sure what values this is referring to */    
    uint64_t nr;                /* @kwinter no documentation on the following fields */
    uint64_t ips[MAX_INSN];
    uint32_t size;              /* Perf sample raw */
    void *data;                 /* Raw data */
};

typedef struct perf_sample perf_sample_t;