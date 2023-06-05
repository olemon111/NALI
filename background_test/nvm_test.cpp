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

// g++ nvm_test.cpp -lpthread -lpmem -o nvm_test
// ./nvm_test thread_nr is_write is_remote test_2nd
/**
 * write
 * sudo ./nvm_test 1 1 0 0
 * sudo ./nvm_test 1 1 1 0
 * 
 * read
 * sudo ./nvm_test 1 0 0 1
 * sudo ./nvm_test 1 0 1 1
 */
using namespace std;

const uint64_t DATA_SIZE = 1UL*1024*1024*1024;
uint64_t IO_SIZE = 32;
uint64_t STRIDE_SIZE = IO_SIZE;

int thread_nums = 1;
bool is_remote = false;
bool is_write = true;
bool test_2nd = false;
bool rand_test = false; // random read/write

atomic<int> barrier(0);

thread_local char *data_array;

// vector<int> stride = {32, 64};
// vector<int> test_size = {32*1024, 64*1024, 256*1024, 512*1024, 2*1024*1024, 8*1024*1024, 16*1024*1024, 64*1024*1024};
vector<uint64_t> stride = {STRIDE_SIZE};
vector<uint64_t> test_size = {DATA_SIZE};

vector<vector<double>> tp_seque(stride.size(), vector<double>(test_size.size(), 0));
vector<vector<double>> latency_seque(stride.size(), vector<double>(test_size.size(), 0));

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

// 初始化方式影响性能
void init_data(char *data_){
    void *v_ptr;
    posix_memalign(&v_ptr, 64, 64);
    volatile double *v_data = NULL;
    v_data = (volatile double *)v_ptr;
    volatile double *p_data = NULL;
    p_data = (volatile double *)data_;

    /* First, make all cache lines in PMEM area dirty */
    // for (int i = 0; i < DATA_SIZE / 8; ++i) {
    //     for (int j = 0; j < 8 >> 3; ++j) {
    //         *(p_data + j) = *(v_data + j);
    //     }
    //     p_data += (8 >> 3);
    // }

    for (size_t i = 0; i < DATA_SIZE / 64; ++i) {
        for (size_t j = 0; j < 64 / sizeof(double); j++) {
            *(p_data + j) = *(v_data + j);
        }
        pmem_persist((void *)p_data, 64);
        p_data += 8;
    }

    /* Flush all cache lines to clear CPU cache */
    clear_cache();
    // p_data = (volatile double *)data_;
    // pmem_persist((void *)p_data, DATA_SIZE);

    // const uint64_t buffer_size = 4UL*1024*1024;
    // char init_buffer[buffer_size];
    // for (int i = 0; i < DATA_SIZE / buffer_size; i++) {
    //     pmem_memcpy_persist(data_ + i, init_buffer, buffer_size);
    // }
} 
 
char seque_read(int test_space, int stride) {
    if (IO_SIZE <= UINT64_MAX) {
        size_t i; 
        int sink; 
        
        for (i = 0; i < test_space - 128*1024; i += stride) { 
            int *addr = (int*)&data_array[i];
            for (int j = 0; j < IO_SIZE / sizeof(int); j++) {
                sink += addr[j];
            }
        }

        return sink;
    }

    // char buffer[IO_SIZE];
    // volatile char res = 0;
    // for (size_t i = 0; i < test_space - 128*1024; i += stride) { 
    //     char *addr = (char*)&data_array[i];
    //     memcpy(buffer, addr, IO_SIZE);
    //     res += buffer[i%IO_SIZE];
    // }
    // return res;
}

char random_read(const vector<uint64_t> &random_array) {
    if (IO_SIZE <= UINT64_MAX) {
        size_t i; 
        int sink; 
        
        for (i = 0; i < random_array.size(); i++) { 
            int *addr = (int*)&data_array[random_array[i]];
            for (int j = 0; j < IO_SIZE / sizeof(int); j++) {
                sink += addr[j];
            }
        }

        return sink;
    }

    // char buffer[IO_SIZE];
    // volatile char res = 0;
    // for (size_t i = 0; i < random_array.size(); i++) { 
    //     char *addr = (char*)&data_array[random_array[i]];
    //     memcpy(buffer, addr, IO_SIZE);
    //     res += buffer[i%IO_SIZE];
    // }
    // return res;
}

void seque_write(int test_space, int stride) { //数组大小，步长
    size_t i;
    char buf[IO_SIZE];
    
    for (i = 0; i < test_space - 128*1024; i += stride) { 
        char *addr = &data_array[i];
        memcpy(addr, buf, IO_SIZE);
        pmem_persist(addr, IO_SIZE);
    } 
}

void random_write(const vector<uint64_t> &random_array) { //随机数数组起始地址，数据数组大小
    size_t i;
    char buf[IO_SIZE];
    
    for (i = 0; i < random_array.size(); i++) {
        char *addr = &data_array[random_array[i]];
        memcpy(addr, buf, IO_SIZE);
        pmem_persist(addr, IO_SIZE);
    }
}

int thread_read_func(size_t thread_id, int s_loc, int num_loc) {
    bindCore(thread_id);
    string pool_name("/mnt/pmem0/zzytest");
    pool_name += to_string(thread_id);
    remove(pool_name.c_str());
    data_array = (char *)pmem_map_file(pool_name.c_str(), DATA_SIZE, PMEM_FILE_CREATE, 0666, NULL, NULL);
    if (data_array == NULL) {
        printf("failed to allocate NVM\n");
        return 1;
    }
    
    init_data(data_array);

    if (is_remote) {
        bindCore(thread_id + 16);
    } else {
        bindCore(thread_id);
    }

    // seq read
    if (test_2nd) {
        seque_read(test_size[num_loc], stride[s_loc]);
        seque_read(test_size[num_loc], stride[s_loc]);
        clear_cache();
    }

    clear_cache();

    // generate random pos array
    vector<uint64_t> random_array;
    for (uint64_t i = 0; i < test_size[num_loc] - 128*1024; i += stride[s_loc] * (rand()%4+1)) { 
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
        tp_seque[s_loc][num_loc] += IO_SIZE * test_size[num_loc] / stride[s_loc] / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        latency_seque[s_loc][num_loc] += time / (test_size[num_loc] / stride[s_loc]);
        res_lock.unlock();
    } else {
        // random read
        clock_gettime(CLOCK_REALTIME, &start);
        random_read(random_array);
        clock_gettime(CLOCK_REALTIME, &end);
        res_lock.lock();
        double time = (end.tv_sec*1000000000 + end.tv_nsec) - (start.tv_sec*1000000000 + start.tv_nsec);
        tp_seque[s_loc][num_loc] += IO_SIZE * random_array.size() / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        latency_seque[s_loc][num_loc] += time / random_array.size();
        res_lock.unlock();
    }

    pmem_unmap(data_array, DATA_SIZE);
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
    data_array = (char *)pmem_map_file(pool_name.c_str(), DATA_SIZE, PMEM_FILE_CREATE, 0666, NULL, NULL);
    if (data_array == NULL) {
        printf("failed to allocate NVM\n");
        return 1;
    }

    // generate random pos array
    vector<uint64_t> random_array;
    for (uint64_t i = 0; i < test_size[num_loc] - 128*1024; i += stride[s_loc] * (rand()%4+1)) { 
        random_array.push_back(i);
    }
    // shuffle
    std::random_device rd;
    std::mt19937 rng(rd());
    std::shuffle(random_array.begin(), random_array.end(), rng);

    // if (test_2nd) {
    //     seque_write(test_size[num_loc] * 2, stride[s_loc]);
    //     clear_cache();
    // }

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
        tp_seque[s_loc][num_loc] += IO_SIZE * test_size[num_loc] / stride[s_loc] / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        latency_seque[s_loc][num_loc] += time / (test_size[num_loc] / stride[s_loc]);
        res_lock.unlock();
    } else {
        // random write
        clock_gettime(CLOCK_REALTIME, &start);
        random_write(random_array);
        clock_gettime(CLOCK_REALTIME, &end);
        res_lock.lock();
        double time = (end.tv_sec*1000000000 + end.tv_nsec) - (start.tv_sec*1000000000 + start.tv_nsec);
        tp_seque[s_loc][num_loc] += IO_SIZE * random_array.size() / 1024.0 / 1024.0 / 1024.0 / time * 1000000000;
        latency_seque[s_loc][num_loc] += time / random_array.size();
        res_lock.unlock();
    }

    pmem_unmap(data_array, DATA_SIZE);
    data_array = NULL;
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc == 6) {
        thread_nums = std::atoi(argv[1]);
        is_write = std::atoi(argv[2]);
        is_remote = std::atoi(argv[3]);
        test_2nd = std::atoi(argv[4]); 
        IO_SIZE = std::atoi(argv[5]);
        printf("thread_nr: %d is_write: %d is_remote : %d test_2nd: %d io_size: %ld\n", thread_nums,
                is_write, is_remote, test_2nd, IO_SIZE);
    } else if (argc != 1) {
        printf("Usage: ./nvm_test thread_nr is_write is_remote test_2nd\n");
        exit(-1);
    } else {
        printf("Use default parameter\n");
    }

    vector<thread> thr(thread_nums);

    barrier = 0;

    for (int i = 0; i < stride.size(); i++) {
        for (int j = 0; j < test_size.size(); j++) {
            tp_seque[i][j] = 0;
            latency_seque[i][j] = 0;
        }
    }

    for (int i = 0; i < stride.size(); i++) {
        for (int j = 0; j < test_size.size(); j++) {
            for(int k = 0; k < thread_nums; k++) {
                if (is_write) {
                    thr[k] = thread(thread_write_func, k, i, j);
                } else {
                    thr[k] = thread(thread_read_func, k, i, j);
                }
            }

            for(int k = 0; k < thread_nums; k++) {
                thr[k].join();
            }

            // cout << "tp: " << tp_seque[0][0] << std::endl; 
            cout << "latency: " << latency_seque[0][0] / thread_nums << std::endl;
        }
    }
    
    return 0;
}
