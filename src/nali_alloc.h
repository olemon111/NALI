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

void bindCore(uint16_t core) {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        printf("can't bind core %d!", core);
        exit(-1);
    }
}

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

static inline int get_numa_id(size_t thread_id) {
    return numa_map[thread_id];
}

static inline char *alloc_new_ppage(size_t thread_id, int32_t ppage_id, bool is_gcfile) {
    // alloc numa local page
    // if is gc thread file, file_name = nali_gc_$(thread_id)_$(ppage_id)
    // else, file_name = nali_$(thread_id)_$(ppage_id)
    int numa_id = get_numa_id(thread_id);
    std::string file_name = "/mnt/pmem" + std::to_string(numa_id) + "/zzy/nali_";
    if (is_gcfile) {
        file_name += "gc_";
    }
    file_name += std::to_string(thread_id) + "_" + std::to_string(ppage_id);
    if (FileExists(file_name.c_str())) {
        std::cout << "nvm file exists " << file_name << std::endl;
        exit(1);
    }

    size_t mapped_len;
    int is_pmem;
    void *ptr = nullptr;
    if((ptr = pmem_map_file(file_name.c_str(), PPAGE_SIZE, PMEM_FILE_CREATE, 0666,
                             &mapped_len, &is_pmem)) == NULL) {
      std::cout << "mmap pmem file fail" << std::endl;
      exit(1);
    }

    // init ppage metadata
    pmem_memset_persist(ptr, 0, 64);

    return (char*)ptr;
}

}