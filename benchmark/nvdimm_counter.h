// apt install libipmctl-dev
// gcc -o nvdimm_counter nvdimm_counter.c -lipmctl
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// libipmctl includes
#include <nvm_types.h>
#include <nvm_management.h>
#include <export_api.h>
#include <NvmSharedDefs.h>

extern struct device_discovery* nvdimms;
extern unsigned int nvdimm_cnt;

static inline void nvdimm_init() {
    nvm_init();

    int status = nvm_get_number_of_devices(&nvdimm_cnt);
    if (status != NVM_SUCCESS) {
        fprintf(stderr, "nvm_get_number_of_devices: %d", status);
        goto error;
    }
    if (nvdimm_cnt <= 0) {
        printf("dimm count is 0!\n");
        goto error;
    }
    printf("dimm count: %d\n", nvdimm_cnt);

    nvdimms = (device_discovery*)malloc(sizeof(struct device_discovery) * nvdimm_cnt);
    status = nvm_get_devices(nvdimms, nvdimm_cnt);
    if (status != NVM_SUCCESS) {
        fprintf(stderr, "nvm_get_devices: %d\n", status);
        free(nvdimms);
        goto error;
    }

    return;

error:
    nvm_uninit();
    exit(-1);
}

static inline void nvdimm_fini() {
    free(nvdimms);
    nvm_uninit();
}

static inline int get_nvdimm_counter(struct device_performance* perf_counter) {
    assert(nvdimms != NULL);
    assert(nvdimm_cnt > 0);
    for (int i = 0; i < nvdimm_cnt; i++) {
        int status = nvm_get_device_performance(nvdimms[i].uid, &perf_counter[i]);
        if (status != NVM_SUCCESS) {
            fprintf(stderr, "nvm_get_device_performance: %d\n", status);
            return -1;
        }
    }
    return 0;
}

// int main(int argc, char** argv) {
//     nvdimm_init();
//     struct device_performance* nvdimm_counter = malloc(sizeof(struct device_performance) * nvdimm_cnt);

//     get_nvdimm_counter(nvdimm_counter);

//     for (int i = 0; i < nvdimm_cnt; i++) {
//         printf("UID: %s, Dev ID: %d\n", nvdimms[i].uid, nvdimms[i].physical_id);
//         printf("  bytes_read:    %llx\n"
//                "  host_reads:    %llx\n"
//                "  bytes_written: %llx\n"
//                "  host_writes:   %llx\n"
//                "  block_reads:   %llx\n"
//                "  block_writes:  %llx\n",
//                nvdimm_counter[i].bytes_read, nvdimm_counter[i].host_reads, nvdimm_counter[i].bytes_written,
//                nvdimm_counter[i].host_writes, nvdimm_counter[i].block_reads, nvdimm_counter[i].block_writes);
//     }

//     free(nvdimm_counter);
//     nvdimm_fini();
// }