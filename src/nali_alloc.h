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
#include <libpmemobj.h>
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
    for (int i = 0; i < 16; i++) {
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

// for test index numa_alloc
// modify from utree source code
static inline PMEMobjpool** init_numa_pool() {
    PMEMobjpool** pop = new PMEMobjpool*[2];
    constexpr size_t pool_size_ = ((size_t)(1024 * 1024 * 128) * 1024);
    const std::string pool_path_ = "/mnt/pmem";
    for (int i = 0; i < numa_node_num; i++) {
        std::string path_ = pool_path_ + std::to_string(i) + "/zzy/nali_data";
        if ((pop[i] = pmemobj_create(path_.c_str(), POBJ_LAYOUT_NAME(btree), pool_size_, 0666)) == NULL) {
            perror("failed to create pool.\n");
            exit(-1);
        }
    }
    return pop;
}

static inline void close_numa_pool(PMEMobjpool** pop) {
    for (int i = 0; i < numa_node_num; i++) {
        pmemobj_close(pop[i]);
    }
    delete [] pop;
}

static inline void *alloc(PMEMobjpool** pop, size_t size) {
    PMEMoid p;
    pmemobj_alloc(pop[get_numa_id(nali::thread_id)], &p, size, 0, NULL, NULL);
    return pmemobj_direct(p);
}

}