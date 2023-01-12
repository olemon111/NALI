#pragma once

#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

#include <string.h>
#include <libpmem.h>
#include "util/utils.h"

// 每个线程分配自己的page用于追加写，每个page 64MB，定期垃圾回收

namespace nali {

const size_t PPAGE_SIZE = 64UL * 1024UL * 1024UL; // 64MB
    
const int numa_max_node = 8; 

const int numa_node_num = 2; // current machine numa nodes

const int max_thread_num = 64; // 每个numa节点32threads

extern thread_local size_t thread_id;
extern int8_t numa_map[max_thread_num];

/**
 * $ lscpu
 * NUMA node0 CPU(s):               0-15,32-47
 * NUMA node1 CPU(s):               16-31,48-63
*/
static inline void init_numa_map() {
    for (int i = 0; i < 8; i++) {
        numa_map[i] = 0;
    }

    for (int i = 8; i < 16; i++) {
        numa_map[i] = 0;
    }

    for (int i = 16; i < 32; i++) {
        numa_map[i] = 1;
    }

    for (int i = 32; i < 48; i++) {
        numa_map[i] = 0;
    }

    for (int i = 48; i < 64; i++) {
        numa_map[i] = 1;
    }
}

static inline int8_t get_numa_id(size_t thread_id) {
    return numa_map[thread_id];
}

}