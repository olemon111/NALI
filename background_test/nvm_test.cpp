#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>
#include <iomanip>
#include <string.h>
#include <libpmem.h>
#include <random>
#include <algorithm>
#include <numa.h>
#include <thread>
#include <mutex>
#include <atomic>

using namespace std;

#define NUM (128 * 1024 * 1024)
#define SIZE 32

int thread_nums = 1;
bool is_remote = false;
bool is_write = false;
bool test_2nd = false;
bool rand_test = false; // random read/write

atomic<int> barrier(0);

vector<atomic_int> fix_read_test_barrier(20);

thread_local char *data_array;

// vector<int> stride = {32, 64};
// vector<int> test_size = {32*1024, 64*1024, 256*1024, 512*1024, 2*1024*1024, 8*1024*1024, 16*1024*1024, 64*1024*1024};
vector<uint64_t> stride = {64};
vector<uint64_t> test_size = {512*1024*1024}; // 64M


vector<vector<double>> tp_seque(stride.size(), vector<double>(test_size.size(), 0));
vector<double> tp_random(test_size.size(), 0);

mutex res_lock;

void clear_cache() {
  // Remove cache
  int size = 256 * 1024 * 1024;
  char *garbage = new char[size];
  for (int i = 0; i < size; ++i)
    garbage[i] = i;
  for (int i = 100; i < size; ++i)
    garbage[i] += garbage[i - 100];
  delete[] garbage;
}

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

void init_data(char *data_, int n){
    void *v_ptr;
    posix_memalign(&v_ptr, 64, 64);
    volatile double *v_data = NULL;
    v_data = (volatile double *)v_ptr;
    volatile double *p_data = NULL;
    p_data = (volatile double *)data_;

    /* First, make all cache lines in PMEM area dirty */
    for (int i = 0; i < NUM * sizeof(double) / 8; ++i) {
        for (int j = 0; j < 8 >> 3; ++j) {
            *(p_data + j) = *(v_data + j);
        }
        p_data += (8 >> 3);
    }

    /* Flush all cache lines to clear CPU cache */
    p_data = (volatile double *)data_;
    pmem_persist((void *)p_data, NUM * sizeof(double));
} 
 
char seque_read(int test_space, int stride) {
    int i; 
    volatile char sink; 
    
    for (i = 0; i < test_space; i += stride) { 
        char *addr = (char*)&data_array[i];
        for (int j = 0; j < SIZE; j++) {
            sink = addr[j];
        }
    }

    return sink;
}

char random_read(const vector<uint64_t> &random_array) {
    int i; 
    volatile char sink; 
    
    for (i = 0; i < random_array.size(); i++) { 
        char *addr = (char*)&data_array[random_array[i]];
        for (int j = 0; j < SIZE; j++) {
            sink = addr[j];
        }
    }

    return sink;
}

void seque_write(int test_space, int stride) { //数组大小，步长
    int i;
    char buf[SIZE];
    
    for (i = 0; i < test_space; i += stride) { 
        char *addr = &data_array[i];
        memcpy(addr, buf, SIZE);
        pmem_persist(addr, SIZE);
    } 
}

void random_write(const vector<uint64_t> &random_array) { //随机数数组起始地址，数据数组大小
    int i;
    char buf[SIZE];
    
    for (i = 0; i < random_array.size(); i++) {
        char *addr = &data_array[random_array[i]];
        memcpy(addr, buf, SIZE);
        pmem_persist(addr, SIZE);
    }
}

int thread_mix_numa_read_func(size_t thread_id, int s_loc, int num_loc) {
    bindCore(thread_id);
    string pool_name("/mnt/pmem0/zzytest");
    pool_name += to_string(thread_id);
    remove(pool_name.c_str());
    data_array = (char *)pmem_map_file(pool_name.c_str(), (NUM + 4) * sizeof(double), PMEM_FILE_CREATE, 0666, NULL, NULL);
    if (data_array == NULL) {
        printf("failed to allocate NVM\n");
        return 1;
    }
    
    init_data(data_array, NUM);

    for (int swap = 1; swap < 10; swap++) {
        bindCore(thread_id + (swap % 2) * 16);

        fix_read_test_barrier[swap]++;
        while(fix_read_test_barrier[swap] < thread_nums) {
            // std::this_thread::sleep_for(std::chrono::milliseconds(100));
            ;
        }

        if (thread_id == 0) {
            std::cout << "times" << swap-1 << std::endl;
        }

        struct timespec start, end, end2;
        // seq read
        clock_gettime(CLOCK_REALTIME, &start);
        seque_read(test_size[num_loc], stride[s_loc]);
        clock_gettime(CLOCK_REALTIME, &end);
        res_lock.lock();
        double time = (end.tv_sec*1000000000 + end.tv_nsec) - (start.tv_sec*1000000000 + start.tv_nsec);
        tp_seque[s_loc][num_loc] += SIZE * test_size[num_loc] / stride[s_loc] / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        res_lock.unlock();

        if (thread_id == 0) {
            std::cout << "numa" << swap % 2 << ": " << tp_seque[s_loc][num_loc] << std::endl;
            tp_seque[s_loc][num_loc] = 0;
        }

        sleep(5);
    }
    
    pmem_unmap(data_array, (NUM + 4) * sizeof(double));
    data_array = NULL;
    return 0;
}

int thread_read_func(size_t thread_id, int s_loc, int num_loc) {
    bindCore(thread_id);
    string pool_name("/mnt/pmem0/zzytest");
    pool_name += to_string(thread_id);
    remove(pool_name.c_str());
    data_array = (char *)pmem_map_file(pool_name.c_str(), (NUM + 4) * sizeof(double), PMEM_FILE_CREATE, 0666, NULL, NULL);
    if (data_array == NULL) {
        printf("failed to allocate NVM\n");
        return 1;
    }
    
    init_data(data_array, NUM);

    if (is_remote) {
        bindCore(thread_id + 16);
    } else {
        bindCore(thread_id);
    }

    // seq read
    if (test_2nd) {
        seque_read(test_size[num_loc] * 2, stride[s_loc]);
        seque_read(test_size[num_loc] * 2, stride[s_loc]);
        clear_cache();
    }

    // generate random pos array
    vector<uint64_t> random_array;
    for (uint64_t i = 0; i < test_size[num_loc]; i += stride[s_loc]) { 
        random_array.push_back(i);
    }
    // shuffle
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(random_array.begin(), random_array.end(), rng);

    barrier++;
    while(barrier < thread_nums) {
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ;
    }

    struct timespec start, end, end2;

    if (!rand_test) {
        // seq read
        clock_gettime(CLOCK_REALTIME, &start);
        seque_read(test_size[num_loc], stride[s_loc]);
        clock_gettime(CLOCK_REALTIME, &end);
        res_lock.lock();
        double time = (end.tv_sec*1000000000 + end.tv_nsec) - (start.tv_sec*1000000000 + start.tv_nsec);
        tp_seque[s_loc][num_loc] += SIZE * test_size[num_loc] / stride[s_loc] / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        res_lock.unlock();
    } else {
        // random read
        clock_gettime(CLOCK_REALTIME, &start);
        random_read(random_array);
        clock_gettime(CLOCK_REALTIME, &end);
        res_lock.lock();
        double time = (end.tv_sec*1000000000 + end.tv_nsec) - (start.tv_sec*1000000000 + start.tv_nsec);
        tp_seque[s_loc][num_loc] += SIZE * test_size[num_loc] / stride[s_loc] / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        res_lock.unlock();
    }

    pmem_unmap(data_array, (NUM + 4) * sizeof(double));
    data_array = NULL;
    return 0;
}

int thread_write_func(size_t thread_id, int s_loc, int num_loc) {
    if (is_remote) {
        bindCore(thread_id + 16);
    } else {
        bindCore(thread_id);
    }

    string pool_name("/mnt/pmem0/zzytest");
    pool_name += to_string(thread_id);
    remove(pool_name.c_str());
    data_array = (char *)pmem_map_file(pool_name.c_str(), (NUM + 4) * sizeof(double), PMEM_FILE_CREATE, 0666, NULL, NULL);
    if (data_array == NULL) {
        printf("failed to allocate NVM\n");
        return 1;
    }
    
    // init_data(data_array, NUM);

    // generate random pos array
    vector<uint64_t> random_array;
    for (uint64_t i = 0; i < test_size[num_loc]; i += stride[s_loc] * 4) { 
        random_array.push_back(i);
    }
    // shuffle
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(random_array.begin(), random_array.end(), rng);

    if (test_2nd) {
        seque_write(test_size[num_loc] * 2, stride[s_loc]);
        clear_cache();
    }

    barrier++;
    while(barrier < thread_nums) {
        // std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ;
    }

    struct timespec start, end, end2;

    if (!rand_test) {
        // seq write
        clock_gettime(CLOCK_REALTIME, &start);
        seque_write(test_size[num_loc], stride[s_loc]);
        clock_gettime(CLOCK_REALTIME, &end);
        res_lock.lock();
        double time = (end.tv_sec*1000000000 + end.tv_nsec) - (start.tv_sec*1000000000 + start.tv_nsec);
        tp_seque[s_loc][num_loc] += SIZE * test_size[num_loc] / stride[s_loc] / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        res_lock.unlock();
    } else {
        // random write
        clock_gettime(CLOCK_REALTIME, &start);
        random_write(random_array);
        clock_gettime(CLOCK_REALTIME, &end);
        res_lock.lock();
        double time = (end.tv_sec*1000000000 + end.tv_nsec) - (start.tv_sec*1000000000 + start.tv_nsec);
        tp_seque[s_loc][num_loc] += SIZE * random_array.size() / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        tp_random[num_loc] = time / test_size[num_loc];
        res_lock.unlock();
    }

    pmem_unmap(data_array, (NUM + 4) * sizeof(double));
    data_array = NULL;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Usage: ./ljr_nvm_test thread_nr is_write is_remote test_2nd\n");
        exit(-1);
    }

    thread_nums = std::atoi(argv[1]);
    is_write = std::atoi(argv[2]);
    is_remote = std::atoi(argv[3]);
    test_2nd = std::atoi(argv[4]);

    vector<thread> thr(thread_nums);

    // // simple mix numa test
    // {
    //     for (int i = 0; i < fix_read_test_barrier.size(); i++) {
    //         fix_read_test_barrier[i] = 0;
    //     }

    //     for (int i = 0; i < stride.size(); i++) {
    //         for (int j = 0; j < test_size.size(); j++) {
    //             tp_seque[i][j] = 0;
    //             tp_random[j] = 0;
    //         }
    //     }

    //     for (int i = 0; i < stride.size(); i++) {
    //         for (int j = 0; j < test_size.size(); j++) {
    //             for(int k = 0; k < thread_nums; k++) {
    //                 thr[k] = thread(thread_mix_numa_read_func, k, i, j);
    //             }

    //             for(int k = 0; k < thread_nums; k++) {
    //                 thr[k].join();
    //             }
    //         }
    //     }
    //     return 0;
    // }
    
    if (!is_write) {
        // read test
        barrier = 0;

        for (int i = 0; i < stride.size(); i++) {
            for (int j = 0; j < test_size.size(); j++) {
                tp_seque[i][j] = 0;
                tp_random[j] = 0;
            }
        }

        for (int i = 0; i < stride.size(); i++) {
            for (int j = 0; j < test_size.size(); j++) {
                for(int k = 0; k < thread_nums; k++) {
                    thr[k] = thread(thread_read_func, k, i, j);
                }

                for(int k = 0; k < thread_nums; k++) {
                    thr[k].join();
                }
            }
        }
    } else {
        // write test
        barrier = 0;
        for (int i = 0; i < stride.size(); i++) {
            for (int j = 0; j < test_size.size(); j++) {
                tp_seque[i][j] = 0;
                tp_random[j] = 0;
            }
        }

        for (int i = 0; i < stride.size(); i++) {
            for (int j = 0; j < test_size.size(); j++) {
                for(int k = 0; k < thread_nums; k++) {
                    thr[k] = thread(thread_write_func, k, i, j);
                }

                for(int k = 0; k < thread_nums; k++) {
                    thr[k].join();
                }
            }
        }
    }

    cout << tp_seque[0][0] << std::endl; 
    
    return 0;
}