#include <iostream>
#include <fstream>
#include <cassert>
#include <iomanip>
#include <sstream>
#include <thread>
#include <getopt.h>
#include <unistd.h>
#include <cmath>
#include <vector>
#include <set>
#include <map>
#include <atomic>
#include <x86intrin.h>

#include "utils.h"
#include "logging.h"
#include "logdb.h"
#include "db_interface.h"
#include "util/sosd_util.h"

#ifdef STATISTIC_PMEM_USE
std::atomic<size_t> pmem_alloc_bytes(0);
#endif

#ifdef USE_CACHE
uint64_t hit_cnt[64];
#endif

// #define STATISTIC_PMEM_INFO

#define NAP_OURS_CMP_TEST
bool zipfan_numa_test_flag = false;

#define PERF_TEST
#define USE_BULKLOAD

// #define ONLY_INSERT
#define LOAD_PHASE
#ifndef ONLY_INSERT
  // #define RECOVERY_TEST
    #ifndef RECOVERY_TEST
      #define ZIPFAN_TEST
      #ifdef ZIPFAN_TEST
      // #define ZIPFAN_UPDATE_TEST
      #endif
      // #define MIX_UPDATE_TEST
      // #define MIX_INSERT_TEST
      #define GET_TEST
      // #define UPDATE_TEST
      // #define SCAN_TEST
      // #define DELETE_TEST
    #endif
#endif

#ifdef STASTISTIC_NALI_CDF
size_t nali::test_total_keys = 0;
#endif

using KEY_TYPE = size_t;

#ifdef VARVALUE
using VALUE_TYPE = std::string;
size_t VALUE_LENGTH = 1024;
#else
using VALUE_TYPE = size_t;
size_t VALUE_LENGTH = 8;
#endif

int8_t global_numa_map[64];
thread_local size_t global_thread_id = -1;

bool begin_gc = false;
std::atomic<size_t> gc_complete(0);

size_t PER_THREAD_POOL_THREADS = 0;
size_t NALI_VERSION_SHARDS = 128;
double GC_THRESHOLD = 0.1; // 0.1 0.3 0.5 0.7 0.9
std::string dataset_path = "/home/zzy/dataset/generate_random_ycsb.dat";

int numa0_thread_num = 16;
int numa1_thread_num = 16;
size_t BULKLOAD_SIZE = 10000000;
size_t LOAD_SIZE   = 200000000;
size_t PUT_SIZE    = 10000000;
size_t GET_SIZE    = 100000000;
size_t UPDATE_SIZE = 10000000;
size_t DELETE_SIZE = 10000000;
size_t ZIPFAN_SIZE = 20000000;
size_t MIX_SIZE    = 10000000;
int Loads_type = 3;

#ifdef STATISTIC_PMEM_INFO
#include "nvdimm_counter.h"
void intel_pin_start() {}
void intel_pin_stop() {}
struct device_discovery* nvdimms = nullptr;
unsigned int nvdimm_cnt = 0;
struct device_performance* nvdimm_counter_begin = nullptr;
struct device_performance* nvdimm_counter_end = nullptr;

static inline void pin_start(struct device_performance** perf_counter) {
  nvdimm_init();
  if (*perf_counter == nullptr) {
    assert(nvdimm_cnt != 0);
    *perf_counter = (device_performance*)malloc(sizeof(struct device_performance) * nvdimm_cnt);
  }
  get_nvdimm_counter(*perf_counter);
  nvdimm_fini();
  intel_pin_start();
}

static inline void pin_end(struct device_performance** perf_counter) {
  intel_pin_stop();

  nvdimm_init();
  if (*perf_counter == nullptr) {
    assert(nvdimm_cnt != 0);
    *perf_counter = (device_performance*)malloc(sizeof(struct device_performance) * nvdimm_cnt);
  }
  get_nvdimm_counter(*perf_counter);
  nvdimm_fini();
}

static inline void print_counter_change(const struct device_performance* perf_counter_begin, 
    const struct device_performance* perf_counter_stop) {
    for (int i = 0; i < nvdimm_cnt; i++) {
      printf("pmem%d, bytes_read: %llx \n", i, perf_counter_stop[i].bytes_read - perf_counter_begin[i].bytes_read);
      printf("pmem%d, host_reads: %llx \n", i, perf_counter_stop[i].host_reads - perf_counter_begin[i].host_reads);
      printf("pmem%d, bytes_written: %llx \n", i, perf_counter_stop[i].bytes_written - perf_counter_begin[i].bytes_written);
      printf("pmem%d, host_writes: %llx \n", i, perf_counter_stop[i].host_writes - perf_counter_begin[i].host_writes);
      printf("-------------------------------\n");
    }
}
#endif

// 实时获取程序占用的内存，单位：kb。
size_t physical_memory_used_by_process()
{
  FILE* file = fopen("/proc/self/status", "r");
  int result = -1;
  char line[128];

  while (fgets(line, 128, file) != nullptr) {
    if (strncmp(line, "VmRSS:", 6) == 0) {
      int len = strlen(line);

      const char* p = line;
      for (; std::isdigit(*p) == false; ++p) {}

      line[len - 3] = 0;
      result = atoi(p);

      break;
    }
  }

  fclose(file);
  return result;
}

template<typename T>
std::vector<T>load_data_from_osm(const std::string dataname)
{
  return util::load_data<T>(dataname);
}

void show_help(char* prog) {
  std::cout <<
    "Usage: " << prog << " [options]" << std::endl <<
    std::endl <<
    "  Option:" << std::endl <<
    "    --thread[-t]             thread number" << std::endl <<
    "    --load-size              LOAD_SIZE" << std::endl <<
    "    --put-size               PUT_SIZE" << std::endl <<
    "    --get-size               GET_SIZE" << std::endl <<
    "    --workload               WorkLoad" << std::endl <<
    "    --help[-h]               show help" << std::endl;
}

int main(int argc, char *argv[]) {
    // uint64_t a = 64000000;
    // for (int i = 1; i <= 16; i++) {
    //   int v = a / (16 + i);
    //   printf("%d\n", v*i);
    // }
    // return 0;

    static struct option opts[] = {
  /* NAME               HAS_ARG            FLAG  SHORTNAME*/
    // {"thread",          required_argument, NULL, 't'},
    {"thread-nums",    required_argument, NULL, 0},
    {"load-size",       required_argument, NULL, 0},
    {"put-size",        required_argument, NULL, 0},
    {"get-size",        required_argument, NULL, 0},
    {"dbname",          required_argument, NULL, 0},
    {"workload",        required_argument, NULL, 0},
    {"loadstype",       required_argument, NULL, 0},
    {"valuesize",       required_argument, NULL, 0},
    {"bgthreads",       required_argument, NULL, 0},
    {"hashshards",      required_argument, NULL, 0},
    {"help",            no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int c;
  int opt_idx;
  std::string  dbName= "alexol";
  std::string  load_file= "";
  int total_thread_num = 16;
  int recovery_threads_num = 0;
  while ((c = getopt_long(argc, argv, "s:dh", opts, &opt_idx)) != -1) {
    switch (c) {
      case 0:
        switch (opt_idx) {
          case 0: total_thread_num = atoi(optarg); break;
          case 1: LOAD_SIZE = atoi(optarg); break;
          case 2: PUT_SIZE = atoi(optarg); break;
          case 3: GET_SIZE = atoi(optarg); break;
          case 4: dbName = optarg; break;
          case 5: load_file = optarg; break;
          case 6: Loads_type = atoi(optarg); break;
          case 7: VALUE_LENGTH = atoi(optarg); break;
          case 8: PER_THREAD_POOL_THREADS = atoi(optarg); break;
          case 9: NALI_VERSION_SHARDS = atoi(optarg); break;
          case 10: show_help(argv[0]); return 0;
          default: std::cerr << "Parse Argument Error!" << std::endl; abort();
        }
        break;
      case 'h': show_help(argv[0]); return 0;
      case '?': break;
      default:  std::cout << (char)c << std::endl; abort();
    }
  }
    // 测1->32线程
  // if (total_thread_num <= 16) {
  //   numa0_thread_num = total_thread_num;
  //   numa1_thread_num = 0;
  // } else {
  //   numa0_thread_num = 16;
  //   numa1_thread_num = total_thread_num - numa0_thread_num;
  // }
  recovery_threads_num = total_thread_num;
  // 测2,4,6->32线程
  if (total_thread_num <= 16) {
    numa0_thread_num = total_thread_num;
    numa1_thread_num = total_thread_num;
    total_thread_num *= 2;
  }
  // dbName = "nap";
#ifdef NAP_OURS_CMP_TEST
  // 使用nap时，不LOAD_PHASE, putsize为16M,zipfan为64M
  if (dbName == "nap") {
    BULKLOAD_SIZE = 1;
    LOAD_SIZE = 0;
    PUT_SIZE = 16000000;
    ZIPFAN_SIZE = 64000000;
    MIX_SIZE=0;
  } else if (dbName == "alexol" || dbName == "nap-nali") {
    LOAD_SIZE = 0;
    PUT_SIZE = 6000000;
    ZIPFAN_SIZE = 64000000;
    MIX_SIZE=0;
  }
#endif
  // pactree不能测试LGT数据集
  if (Loads_type == 5 && dbName == "pactree")
    return 0;

  std::cout << "NUMA0 THREAD NUMBER:   " << numa0_thread_num << std::endl;
  std::cout << "NUMA1 THREAD NUMBER:   " << numa1_thread_num << std::endl;
  std::cout << "BULKLOAD_SIZE:         " << BULKLOAD_SIZE << std::endl;
  std::cout << "LOAD_SIZE:             " << LOAD_SIZE << std::endl;
  std::cout << "PUT_SIZE:              " << PUT_SIZE << std::endl;
  std::cout << "GET_SIZE:              " << GET_SIZE << std::endl;
  std::cout << "UPDATE_SIZE:           " << UPDATE_SIZE << std::endl;
  std::cout << "DELETE_SIZE:           " << DELETE_SIZE << std::endl;
  std::cout << "ZIPFAN_SIZE:           " << ZIPFAN_SIZE << std::endl;
  std::cout << "DB  name:              " << dbName << std::endl;
  std::cout << "Workload:              " << load_file << std::endl;
#ifdef VARVALUE
  std::cout << "valsize:               " << VALUE_LENGTH << std::endl;
#else
  std::cout << "valsize:               " << 8 << std::endl;
#endif
  std::cout << "hash_shards:           " << NALI_VERSION_SHARDS << std::endl;
  std::cout << "bgthreads:             " << PER_THREAD_POOL_THREADS << std::endl;

  std::vector<uint64_t> data_base;
  switch (Loads_type)
  {
    case 3:
      data_base = load_data_from_osm<uint64_t>("/home/zzy/dataset/generate_random_ycsb.dat");
      break;
    case 4:
      data_base = load_data_from_osm<uint64_t>("/home/zzy/dataset/generate_random_osm_longlat.dat");
      break;
    case 5:
      data_base = load_data_from_osm<uint64_t>("/home/zzy/dataset/generate_random_osm_longtitudes.dat");
      break;
    case 6:
      data_base = load_data_from_osm<uint64_t>("/home/zzy/dataset/lognormal.dat");
      break;
    default:
      LOG_INFO("not defined loads_type");
      assert(false);
      break;
  }

  LOG_INFO("@@@@@@@@@@@@ Init @@@@@@@@@@@@");

  // generate thread_ids
  assert(numa0_thread_num >= 0 && numa0_thread_num <=16);
  assert(numa1_thread_num >= 0 && numa1_thread_num <=16);
  std::vector<std::vector<int>> thread_ids(nali::numa_max_node, std::vector<int>());
  std::vector<int> thread_id_arr;
  for (int i = 0; i < numa0_thread_num; i++) {
    thread_ids[0].push_back(i);
    thread_id_arr.push_back(i);
  }
  for (int i = 0; i < numa1_thread_num; i++) {
    thread_ids[1].push_back(16+i);
    thread_id_arr.push_back(16+i);
  }

#ifdef RECOVERY_TEST
  Tree<size_t, uint64_t> *real_db = new nali::alexoldb<size_t, uint64_t>();
  {
    BULKLOAD_SIZE = 10000000;
    auto values = new std::pair<KEY_TYPE, uint64_t>[BULKLOAD_SIZE];
    size_t sp = 250000000;
    for (int i = 0; i < BULKLOAD_SIZE; i++) {
      if (Loads_type == 6) {
        sp = 120000000;
      }
      values[i].first = data_base[i+sp];
      values[i].second = data_base[i+sp];
    }
    std::sort(values, values + BULKLOAD_SIZE, [&](auto const& a, auto const& b) { return a.first < b.first; });
    real_db->bulk_load(values, BULKLOAD_SIZE);
    delete [] values;
  }
  std::cout << "start recovery" << std::endl;
  auto ts = TIME_NOW;
  Tree<KEY_TYPE, VALUE_TYPE> *db = new nali::logdb<KEY_TYPE, VALUE_TYPE>(real_db, true, recovery_threads_num);
  auto te = TIME_NOW;
  auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
  std::cout << "[Recovery time]: " << use_seconds << " s" << std::endl;
  return 0;
#else
  size_t init_dram_space_use = physical_memory_used_by_process();
  std::cout << "before newdb, dram space use: " << init_dram_space_use / 1024.0 /1024.0  << " GB" << std::endl;

  Tree<KEY_TYPE, VALUE_TYPE> *db = nullptr;
  if  (dbName == "alexol") {
    Tree<size_t, uint64_t> *real_db = new nali::alexoldb<size_t, uint64_t>();
    db = new nali::logdb<KEY_TYPE, VALUE_TYPE>(real_db);
  }
  // else if (dbName == "nali") {
  //   real_db = new nali::nalidb<size_t, uint64_t>();
  //   db = new nali::logdb<KEY_TYPE, VALUE_TYPE>(real_db);
  // } 
  // else if (dbName == "utree") {
  //   db = new nali::utree_db<KEY_TYPE, VALUE_TYPE>();
  // } 
  // else if (dbName == "fastfair") {
  //   db = new nali::fastfair_db<KEY_TYPE, VALUE_TYPE>();
  // }
  // else if (dbName == "pactree") {
  //   db = new nali::pactree_db<KEY_TYPE, VALUE_TYPE>(2);
  // } 
  // else if (dbName == "dptree") {
  //   db = new nali::dptree_db<KEY_TYPE, VALUE_TYPE>(total_thread_num);
  // } 
  else if (dbName == "nap") {
    db = new nali::napfastfair_db<KEY_TYPE, VALUE_TYPE>();
  }
  else if (dbName == "nap-nali")
  {
    db = new nali::napnali_db<KEY_TYPE, VALUE_TYPE>();
  }
   else {
    LOG_INFO("not defined db: %s", dbName.c_str());
    assert(false);
  }

  #ifdef USE_BULKLOAD
  // alexol/apex must bulkload 10M/1M sorted kv
  {
    #ifdef STATISTIC_PMEM_INFO
    pin_start(&nvdimm_counter_begin);
    #endif
    auto values = new std::pair<KEY_TYPE, VALUE_TYPE>[BULKLOAD_SIZE];
    #ifdef VARVALUE
    std::string value(VALUE_LENGTH, '1');
    #endif
    for (int i = 0; i < BULKLOAD_SIZE; i++) {
      values[i].first = data_base[i];
      #ifdef VARVALUE
      #ifndef PERF_TEST
      memcpy((char *)value.c_str(), &data_base[i], 8);
      #endif
      values[i].second = value;
      #else
      values[i].second = data_base[i];
      #endif
    }

    if (dbName == "alexol" || dbName == "apex" || dbName == "nap-nali") {
      std::sort(values, values + BULKLOAD_SIZE,
        [&](auto const& a, auto const& b) { return a.first < b.first; });
    }

    LOG_INFO("@@@@ BULK LOAD START @@@@");
    global_thread_id = 0; // thread0 do bulkload
    bindCore(global_thread_id);
    init_dram_space_use = physical_memory_used_by_process();
    db->bulk_load(values, BULKLOAD_SIZE);
    std::cout << "after bulkload: dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 /1024.0  << " GB" << std::endl;
    LOG_INFO("@@@@ BULK LOAD END @@@@");
    #ifdef STATISTIC_PMEM_INFO
    pin_end(&nvdimm_counter_end);
    // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif

    delete [] values;
  }
  #endif
  std::cout << "dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 /1024.0  << " GB" << std::endl;
#ifdef LOAD_PHASE
  {
    LOG_INFO(" @@@@@@@@@@@@@ LOAD @@@@@@@@@@@@@@@");

    #ifdef STATISTIC_PMEM_INFO
    pin_start(&nvdimm_counter_begin);
    #endif

    #ifdef STASTISTIC_NALI_CDF
    nali::test_total_keys = LOAD_SIZE;
    #endif

    // Load
    auto ts = TIME_NOW;

    std::vector<std::thread> threads;
    std::atomic<uint64_t> thread_idx_count(0);
    size_t per_thread_size = LOAD_SIZE / total_thread_num;
    for(int i = 0; i < thread_id_arr.size(); i++) {
      threads.emplace_back([&](){
        size_t idx = thread_idx_count.fetch_add(1); 
        global_thread_id = thread_id_arr[idx];
        bindCore(global_thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (LOAD_SIZE-idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size + BULKLOAD_SIZE;
        #ifdef VARVALUE
        std::string value(VALUE_LENGTH, '1');
        #endif
        for (size_t j = 0; j < size; ++j) {
          #ifdef VARVALUE
          #ifndef PERF_TEST
          memcpy((char *)value.c_str(), &data_base[start_pos+j], 8);
          #endif
          bool ret = db->insert(data_base[start_pos+j], value);
          #else
          bool ret = db->insert(data_base[start_pos+j], data_base[start_pos+j]);
          #endif
          if(idx == 0 && (j + 1) % 10000000 == 0) {
            std::cout << "Operate: " << j + 1 << std::endl;
            std::cout << "dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 /1024.0  << " GB" << std::endl;
          }
        }
      });
    }

    for (auto& t : threads)
      t.join();

    auto te = TIME_NOW;

    #ifdef STATISTIC_PMEM_INFO
    pin_end(&nvdimm_counter_end);
    // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif

    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Load]: Load " << LOAD_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(LOAD_SIZE)/(double)use_seconds << " ." << std::endl;
    std::cout << "dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 /1024.0  << " GB" << std::endl;
  }
#endif

  #ifdef STASTISTIC_NALI_CDF
  db->get_info();
  exit(0);
  #endif

  {
    // Put
    LOG_INFO(" @@@@@@@@@@@@@ put @@@@@@@@@@@@@@@");
    std::vector<std::thread> threads;
    std::atomic<uint64_t> thread_idx_count(0);
    size_t per_thread_size = PUT_SIZE / total_thread_num;

    #ifdef STATISTIC_PMEM_INFO
    // pin_start(&nvdimm_counter_begin);
    #endif
    
    auto ts = TIME_NOW;

    for(int i = 0; i < thread_id_arr.size(); i++) {
      threads.emplace_back([&](){
        size_t idx = thread_idx_count.fetch_add(1); 
        global_thread_id = thread_id_arr[idx];
        bindCore(global_thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (PUT_SIZE - idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size + LOAD_SIZE + BULKLOAD_SIZE;
        #ifdef VARVALUE
        std::string value(VALUE_LENGTH, '1');
        #endif
        for (size_t j = 0; j < size; ++j) {
          #ifdef VARVALUE
          #ifndef PERF_TEST
          memcpy((char *)value.c_str(), &data_base[start_pos+j], 8);
          #endif
          auto ret = db->insert(data_base[start_pos+j], value);
          #else
          auto ret = db->insert(data_base[start_pos+j], data_base[start_pos+j]);
          #endif
          // if (ret != 1) {
          //     std::cout << "Put error, key: " << data_base[start_pos+j] << ", size: " << j << std::endl;
          //     assert(0);
          // }
          if(idx == 0 && (j + 1) % 100000 == 0) {
            std::cerr << "Operate: " << j + 1 << '\r'; 
          }
        }
      });
    }

    for (auto& t : threads)
      t.join();
        
    auto te = TIME_NOW;

    #ifdef STATISTIC_PMEM_INFO
    // pin_end(&nvdimm_counter_end);
    // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif

    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Put]: Put " << PUT_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(PUT_SIZE)/use_seconds << " ." << std::endl;
  }

#ifdef STATISTIC_PMEM_USE
  std::cout << "nvm space use: " << pmem_alloc_bytes * 1.0 / 1024.0 / 1024.0 / 1024.0 << " GB" << std::endl;
#endif

#ifndef ONLY_INSERT
#ifdef GET_TEST
  {
    // Get
    Random get_rnd(10000000, LOAD_SIZE+PUT_SIZE+BULKLOAD_SIZE-1);
    // std::random_shuffle(data_base.begin(), data_base.begin()+200000000);
    for (size_t i = 0; i < GET_SIZE; ++i) {
      size_t idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }
    
    LOG_INFO(" @@@@@@@@@@@@@ get @@@@@@@@@@@@@@@");

    for (int loop = 0; loop < 1; loop++) {
      std::vector<std::thread> threads;
      std::atomic<uint64_t> thread_idx_count(0);
      size_t per_thread_size = GET_SIZE / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          size_t idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          // bindCore(global_thread_id % 2 ? global_thread_id : global_thread_id + 16);
          // bindCore(loop % 2 ? global_thread_id+16 : global_thread_id);
          // bindCore(loop % 2 ? global_thread_id: global_thread_id + 16);
          bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (GET_SIZE-idx*per_thread_size) : per_thread_size;
          size_t start_pos = idx * per_thread_size;
              
          int wrong_get = 0;
          #ifdef VARVALUE
            std::string value;
            std::string cmp_value(VALUE_LENGTH, '1');
          #else
            size_t value;
          #endif
          for (int t = 0; t < 1; t++) {
            for (size_t j = 0; j < size; ++j) {
              #ifdef VARVALUE
                auto ret = db->search(data_base[start_pos+j], value);
                #ifndef PERF_TEST
                memcpy((char *)cmp_value.c_str(), &data_base[start_pos+j], 8);
                if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
                  wrong_get++;
                }
                #endif
              #else
                auto ret = db->search(data_base[start_pos+j], value);
                #ifndef PERF_TEST
                if (!ret || (value != data_base[start_pos+j] && value != 0x19990627UL)) {
                  wrong_get++;
                }
                #endif
              #endif
              
              if(idx == 0 && (j + 1) % 100000 == 0) {
                std::cerr << "Operate: " << j + 1 << '\r'; 
              }
            }
          }
          if (wrong_get != 0) {
            std::cout << "thread " << global_thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
          }
        });
      }

      for (auto& t : threads)
        t.join();
          
      auto te = TIME_NOW;

      #ifdef STATISTIC_PMEM_INFO
      pin_end(&nvdimm_counter_end);
      print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Get]: Get " << GET_SIZE << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(GET_SIZE)/use_seconds << " ." << std::endl;
      sleep(5);
    }
  }
#endif

#ifdef UPDATE_TEST
  {
    // update
    Random get_rnd(0, LOAD_SIZE+PUT_SIZE+BULKLOAD_SIZE-1);
    
    for (size_t i = 0; i < UPDATE_SIZE; ++i) {
      size_t idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }

    LOG_INFO(" @@@@@@@@@@@@@ update @@@@@@@@@@@@@@@");

    for (int loop = 0; loop < 1; loop++) {
      std::vector<std::thread> threads;
      std::atomic<uint64_t> thread_idx_count(0);
      size_t per_thread_size = UPDATE_SIZE / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          size_t idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (UPDATE_SIZE-idx*per_thread_size) : per_thread_size;
          size_t start_pos = idx * per_thread_size;
              
          int wrong_get = 0;
          #ifdef VARVALUE
            std::string value;
          #else
            size_t value;
          #endif
          size_t t_v = 0x19990627UL;
          for (size_t j = 0; j < size; ++j) {
            #ifdef VARVALUE
              #ifndef PERF_TEST
              memcpy((char *)value.c_str(), &t_v, 8);
              #endif
              auto ret = db->update(data_base[start_pos+j], value);
            #else
              auto ret = db->update(data_base[start_pos+j], t_v);
            #endif
            
            if(idx == 0 && (j + 1) % 100000 == 0) {
              std::cerr << "Operate: " << j + 1 << '\r'; 
            }
          }
          if (wrong_get != 0) {
            std::cout << "thread " << global_thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
          }
        });
      }

      for (auto& t : threads)
        t.join();
          
      auto te = TIME_NOW;

      #ifdef STATISTIC_PMEM_INFO
      pin_end(&nvdimm_counter_end);
      // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Update]: Update " << UPDATE_SIZE << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(UPDATE_SIZE)/use_seconds << " ." << std::endl;
    }
  }
#endif

#ifdef ZIPFAN_TEST
  {
    // std::vector<float> zipfan_params = {0.5, 0.6, 0.7, 0.8, 0.9, 0.99};
    std::vector<float> zipfan_params = {0.99};
    bool is_read = true;
    
zipfan_test:
    LOG_INFO(" @@@@@@@@@@@@@ zipfan update/get @@@@@@@@@@@@@@@");
    for (int times = 0; times < 1; times++) {
      if (times == 0) {
        zipfan_numa_test_flag = true;
      }
      for (int loop = 0; loop < zipfan_params.size(); loop++) {
        #ifdef USE_CACHE
        memset(hit_cnt, 0, sizeof(hit_cnt));
        #endif
        std::vector<std::thread> threads;
        std::atomic<uint64_t> thread_idx_count(0);
        size_t per_thread_size = ZIPFAN_SIZE / total_thread_num;
        Random get_rnd(10000000, LOAD_SIZE+PUT_SIZE+BULKLOAD_SIZE-1);
        size_t pos_arr_size = 4000000;
        std::vector<size_t> pos_arr(pos_arr_size);
        for (int i = 0; i < pos_arr_size; i++)
          pos_arr[i] = get_rnd.Next();
        size_t *test_union = get_search_keys_zipf_with_theta(pos_arr.data(), pos_arr_size, ZIPFAN_SIZE, zipfan_params[loop]);

        #ifdef STATISTIC_PMEM_INFO
        pin_start(&nvdimm_counter_begin);
        #endif

        auto ts = TIME_NOW;
        for (int i = 0; i < thread_id_arr.size(); ++i) {
            threads.emplace_back([&](){
            size_t idx = thread_idx_count.fetch_add(1); 
            global_thread_id = thread_id_arr[idx];
            bindCore(global_thread_id);
            size_t size = (idx == thread_id_arr.size()-1) ? (ZIPFAN_SIZE-idx*per_thread_size) : per_thread_size;
            size_t start_pos = idx * per_thread_size;
                
            int wrong_get = 0;
            #ifdef VARVALUE
              std::string value(VALUE_LENGTH, '1');
              std::string cmp_value(VALUE_LENGTH, '1');
            #else
              size_t value;
            #endif
            size_t t_v = 0x19990627UL;
            for (int times = 0; times < 1; times++) {
              for (size_t j = 0; j < size; ++j) {
                bool ret;
                if (!is_read) {
                  #ifdef VARVALUE
                    #ifndef PERF_TEST
                    memcpy((char *)value.c_str(), &t_v, 8);
                    #endif
                    ret = db->update(data_base[test_union[start_pos+j]], value);
                  #else
                    ret = db->update(data_base[test_union[start_pos+j]], t_v);
                  #endif
                } else {
                  #ifdef VARVALUE
                    ret = db->search(data_base[test_union[start_pos+j]], value);
                    #ifndef PERF_TEST
                    memcpy((char *)cmp_value.c_str(), &data_base[test_union[start_pos+j]], 8);
                    if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
                      wrong_get++;
                    }
                    #endif
                  #else
                    ret = db->search(data_base[test_union[start_pos+j]], value);
                    #ifndef PERF_TEST
                    if (!ret || (value != data_base[test_union[start_pos+j]] && value != 0x19990627UL)) {
                      wrong_get++;
                    }
                    #endif
                  #endif
                }
                if(idx == 0 && (j + 1) % 100000 == 0) {
                  std::cerr << "Operate: " << j + 1 << '\r'; 
                }
              }
            }
            
            if (wrong_get != 0) {
              std::cout << "thread " << global_thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
            }
          });
        }

        for (auto& t : threads)
          t.join();
            
        auto te = TIME_NOW;

        #ifdef STATISTIC_PMEM_INFO
        pin_end(&nvdimm_counter_end);
        // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
        #endif
        
        if (times == 0) {
          auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
          std::string test_type = is_read ? "[zipfan_read]: " : "[zipfan_update]: ";
          std::cout << test_type << "zipfan" << zipfan_params[loop] << " " << ZIPFAN_SIZE << ": " 
                    << "cost " << use_seconds << "s, " 
                    << "iops " << (double)(ZIPFAN_SIZE)/use_seconds << " ." << std::endl;
          // delete [] zipfan_base;
          #ifdef USE_CACHE
          uint64_t total_hit = 0;
          for (int i = 0; i < 64; ++i) {
            total_hit += hit_cnt[i];
          }
          std::cout << "hit rate: " << total_hit * 1.0 / ZIPFAN_SIZE << std::endl;
          #endif
          db->get_info();
        }
      }
    }
    
    #ifdef ZIPFAN_UPDATE_TEST
    if (is_read) {
      is_read = false;
      goto zipfan_test;
    }
    #endif
  }
#endif

#ifdef MIX_UPDATE_TEST
  {
    size_t MIX_SIZE = 10000000;
    std::vector<float> update_ratios = {0.05, 0.2, 0.5, 0.8, 0.95};
    float update_ratio = 0;

    LOG_INFO(" @@@@@@@@@@@@@ mixed update/get @@@@@@@@@@@@@@@");

    for (int loop = 0; loop < update_ratios.size(); loop++) {
      // mix update/get
      util::FastRandom ranny(loop+1999);
      std::vector<double> random_ratio(MIX_SIZE);
      Random get_rnd(0, LOAD_SIZE+PUT_SIZE-1);
      for (size_t i = 0; i < MIX_SIZE; ++i) {
        size_t idx = get_rnd.Next();
        std::swap(data_base[i], data_base[idx]);
        random_ratio[i] = ranny.ScaleFactor();
      }

      update_ratio = update_ratios[loop];
      std::vector<std::thread> threads;
      std::atomic<uint64_t> thread_idx_count(0);
      size_t per_thread_size = MIX_SIZE / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          size_t idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (MIX_SIZE-idx*per_thread_size) : per_thread_size;
          size_t start_pos = idx * per_thread_size;
              
          int wrong_get = 0;
          #ifdef VARVALUE
            std::string value(VALUE_LENGTH, '1');
            std::string cmp_value(VALUE_LENGTH, '1');
          #else
            size_t value;
          #endif
          size_t t_v = 0x19990627UL;
          for (int t = 0; t < 1; t++) {
            for (size_t j = 0; j < size; ++j) {
              bool ret;
              if (random_ratio[start_pos+j] < update_ratio) {
                #ifdef VARVALUE
                  #ifndef PERF_TEST
                  memcpy((char *)value.c_str(), &t_v, 8);
                  #endif
                  ret = db->update(data_base[start_pos+j], value);
                #else
                  ret = db->update(data_base[start_pos+j], t_v);
                #endif
              } else {
                #ifdef VARVALUE
                  ret = db->search(data_base[start_pos+j], value);
                  #ifndef PERF_TEST
                  memcpy((char *)cmp_value.c_str(), &data_base[start_pos+j], 8);
                  if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
                    wrong_get++;
                  }
                  #endif
                #else
                  ret = db->search(data_base[start_pos+j], value);
                  #ifndef PERF_TEST
                  if (!ret || (value != data_base[start_pos+j] && value != 0x19990627UL)) {
                    wrong_get++;
                  }
                  #endif
                #endif
              }
              
              if(idx == 0 && (j + 1) % 100000 == 0) {
                std::cerr << "Operate: " << j + 1 << '\r'; 
              }
            }
          }
          if (wrong_get != 0) {
            std::cout << "thread " << global_thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
          }
        });
      }

      for (auto& t : threads)
        t.join();
          
      auto te = TIME_NOW;

      #ifdef STATISTIC_PMEM_INFO
      pin_end(&nvdimm_counter_end);
      // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Mixupdate]: Mixupdate" << update_ratio << " " << MIX_SIZE << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(MIX_SIZE)/use_seconds << " ." << std::endl;
    }
  }
#endif

#ifdef MIX_INSERT_TEST
  {
    std::vector<float> insert_ratios = {0.05, 0.2, 0.5, 0.8, 0.95};
    size_t cur_insert_loc = BULKLOAD_SIZE+LOAD_SIZE+PUT_SIZE;
    LOG_INFO(" @@@@@@@@@@@@@ mixed insert/get @@@@@@@@@@@@@@@");
    for (int loop = 0; loop < insert_ratios.size(); loop++) {
      std::vector<std::thread> threads;
      std::atomic<uint64_t> thread_idx_count(0);
      size_t per_thread_size = MIX_SIZE / total_thread_num;
      util::FastRandom ranny(loop+627);
      std::vector<std::pair<bool, int>> op_arr(MIX_SIZE);
      Random get_rnd(0, BULKLOAD_SIZE+LOAD_SIZE+PUT_SIZE-1);
      int gen_pos = cur_insert_loc;
      for (size_t i = 0; i < MIX_SIZE; ++i) {
        float ratio = ranny.ScaleFactor();
        if (ratio < insert_ratios[loop]) {
          op_arr[i] = {true, gen_pos++};
        } else {
          op_arr[i] = {false, get_rnd.Next()};
        }
      }
      cur_insert_loc = gen_pos;

      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          size_t idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (MIX_SIZE-idx*per_thread_size) : per_thread_size;
          size_t start_pos = idx * per_thread_size;
              
          int wrong_get = 0;
          #ifdef VARVALUE
            std::string value(VALUE_LENGTH, '1');
            std::string cmp_value(VALUE_LENGTH, '1');
          #else
            size_t value;
          #endif
          for (size_t j = 0; j < size; ++j) {
            bool ret;
            if (op_arr[start_pos+j].first == true) {
              #ifdef VARVALUE
                #ifndef PERF_TEST
                memcpy((char *)value.c_str(), &(data_base[op_arr[start_pos+j].second]), 8);
                #endif
                ret = db->insert(data_base[op_arr[start_pos+j].second], value);
              #else
                ret = db->insert(data_base[op_arr[start_pos+j].second], data_base[op_arr[start_pos+j].second]);
              #endif
            } else {
              #ifdef VARVALUE
                ret = db->search(data_base[start_pos+j], value);
                #ifndef PERF_TEST
                memcpy((char *)cmp_value.c_str(), &data_base[start_pos+j], 8);
                if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
                  wrong_get++;
                }
                #endif
              #else
                ret = db->search(data_base[op_arr[start_pos+j].second], value);
                #ifndef PERF_TEST
                if (!ret || (value != data_base[start_pos+j] && value != 0x19990627UL)) {
                  wrong_get++;
                }
                #endif
              #endif
            }
            
            if(idx == 0 && (j + 1) % 100000 == 0) {
              std::cerr << "Operate: " << j + 1 << '\r'; 
            }
          }
          if (wrong_get != 0) {
            std::cout << "thread " << global_thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
          }
        });
      }

      for (auto& t : threads)
        t.join();
          
      auto te = TIME_NOW;

      #ifdef STATISTIC_PMEM_INFO
      pin_end(&nvdimm_counter_end);
      // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Mixinsert]: Mixinsert" << insert_ratios[loop] << " " << MIX_SIZE << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(MIX_SIZE)/use_seconds << " ." << std::endl;
    }
  }
#endif
#ifdef SCAN_TEST
  {
    // Scan
    const uint64_t total_scan_ops = 1000000;
    std::vector<int> scan_size = {100};
    
    Random get_rnd(0, BULKLOAD_SIZE+LOAD_SIZE+PUT_SIZE-10000);
    for (size_t i = 0; i < total_scan_ops; ++i) {
      size_t idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }

    LOG_INFO(" @@@@@@@@@@@@@ scan @@@@@@@@@@@@@@@");

    for (int scan : scan_size) {
      std::vector<std::thread> threads;
      std::atomic<uint64_t> thread_idx_count(0);
      size_t per_thread_size = total_scan_ops / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          size_t idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (total_scan_ops-idx*per_thread_size) : per_thread_size;
          size_t start_pos = idx * per_thread_size;
          #ifdef VARVALUE
            auto *results = new std::pair<uint64_t, std::string>[scan];
            std::string cmp_value(VALUE_LENGTH, '1');
          #else
            auto *results = new std::pair<uint64_t, uint64_t>[scan];
          #endif
          int total_get = 0;
          int wrong_get = 0;
          for (int t = 0; t < 1; t++) {
            for (size_t j = 0; j < size; ++j) {
              int scan_num = db->range_scan_by_size(data_base[start_pos+j], scan, results);
              total_get += scan_num;
              #ifndef PERF_TEST
              for (int s = 0; s < scan_num; s++) {
                #ifdef VARVALUE
                  memcpy((char *)cmp_value.c_str(), &results[s].first, 8);
                  if (results[s].second != cmp_value && results[s].second != std::to_string(0x19990627UL)) {
                    wrong_get++;
                  }
                #else
                  if (results[s].first != results[s].second && results[s].second != 0x19990627UL) {
                    wrong_get++;
                  }
                #endif
              }
              #endif
              if(idx == 0 && (j + 1) % 100000 == 0) {
                std::cerr << "Operate: " << j + 1 << '\r'; 
              }
            }
          }
          if (wrong_get != 0) {
            std::cout << "thread " << global_thread_id << ", total scan kvs: " << total_get << ", wrong get: " << wrong_get << std::endl;
          }
          delete [] results;
        });
      }

      for (auto& t : threads)
        t.join();
          
      auto te = TIME_NOW;

      #ifdef STATISTIC_PMEM_INFO
      pin_end(&nvdimm_counter_end);
      // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Scan]: Scan " << total_scan_ops << ": per scan size " << scan << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(total_scan_ops)/use_seconds << " ." << std::endl;
    }
  }
#endif

#ifdef DELETE_TEST
  {
     // Delete
    Random get_rnd(0, BULKLOAD_SIZE+LOAD_SIZE+PUT_SIZE-1);
    for (size_t i = 0; i < DELETE_SIZE; ++i) {
      size_t idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }

    LOG_INFO(" @@@@@@@@@@@@@ delete @@@@@@@@@@@@@@@");

    std::vector<std::thread> threads;
    std::atomic<uint64_t> thread_idx_count(0);
    size_t per_thread_size = DELETE_SIZE / total_thread_num;
    
    #ifdef STATISTIC_PMEM_INFO
    pin_start(&nvdimm_counter_begin);
    #endif

    auto ts = TIME_NOW;
    for (int i = 0; i < thread_id_arr.size(); ++i) {
        threads.emplace_back([&](){
        size_t idx = thread_idx_count.fetch_add(1); 
        global_thread_id = thread_id_arr[idx];
        bindCore(global_thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (DELETE_SIZE-idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size;
            
        int wrong_get = 0;
        for (int t = 0; t < 1; t++) {
          for (size_t j = 0; j < size; ++j) {
            bool ret;
            size_t value;
            ret = db->erase(data_base[start_pos+j]);
            if(idx == 0 && (j + 1) % 100000 == 0) {
              std::cerr << "Operate: " << j + 1 << '\r'; 
            }
          }
        }
        if (wrong_get != 0) {
          std::cout << "thread " << global_thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
        }
      });
    }

    for (auto& t : threads)
      t.join();
        
    auto te = TIME_NOW;

    #ifdef STATISTIC_PMEM_INFO
    pin_end(&nvdimm_counter_end);
    // print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif
    
    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Delete]: Delete " << DELETE_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(DELETE_SIZE)/use_seconds << " ." << std::endl;
  }
#endif
  // #ifdef STATISTIC_PMEM_USE
  // std::cout << "before gc, nvm space use: " << pmem_alloc_bytes * 1.0 / 1024.0 / 1024.0 / 1024.0 << " GB" << std::endl;
  // #endif
  // do_gc
  // begin_gc = true;
  // sleep(5);

  // auto ts = TIME_NOW;
  // for (int i = 1; i < 200 && gc_complete < PER_THREAD_POOL_THREADS; i++) {
  //   sleep(1);
  //   #ifdef STATISTIC_PMEM_USE
  //   std::cout << "gc_gc_gc, nvm space use: " << pmem_alloc_bytes * 1.0 / 1024.0 / 1024.0 / 1024.0 << " GB" << std::endl;
  //   #endif
  // }
  // auto te = TIME_NOW;
  // auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
  // std::cout << "gc_end, use seconds: " << use_seconds << " s" << std::endl;

  // {
  //   // Get
  //   GET_SIZE = 10000000;
  //   Random get_rnd(DELETE_SIZE, LOAD_SIZE+PUT_SIZE+BULKLOAD_SIZE-1);
  //   for (size_t i = 0; i < GET_SIZE; ++i) {
  //     size_t idx = get_rnd.Next();
  //     std::swap(data_base[i], data_base[idx]);
  //   }
    
  //   LOG_INFO(" @@@@@@@@@@@@@ get @@@@@@@@@@@@@@@");

  //   for (int loop = 0; loop < 1; loop++) {
  //     std::vector<std::thread> threads;
  //     std::atomic<uint64_t> thread_idx_count(0);
  //     size_t per_thread_size = GET_SIZE / total_thread_num;
      
  //     #ifdef STATISTIC_PMEM_INFO
  //     pin_start(&nvdimm_counter_begin);
  //     #endif

  //     auto ts = TIME_NOW;
  //     for (int i = 0; i < thread_id_arr.size(); ++i) {
  //         threads.emplace_back([&](){
  //         size_t idx = thread_idx_count.fetch_add(1); 
  //         global_thread_id = thread_id_arr[idx];
  //         bindCore(global_thread_id);
  //         size_t size = (idx == thread_id_arr.size()-1) ? (GET_SIZE-idx*per_thread_size) : per_thread_size;
  //         size_t start_pos = idx * per_thread_size;
              
  //         int wrong_get = 0;
  //         #ifdef VARVALUE
  //           std::string value;
  //           std::string cmp_value(VALUE_LENGTH, '1');
  //         #else
  //           size_t value;
  //         #endif
  //         for (int t = 0; t < 1; t++) {
  //           for (size_t j = 0; j < size; ++j) {
  //             #ifdef VARVALUE
  //               auto ret = db->search(data_base[start_pos+j], value);
  //               #ifndef PERF_TEST
  //               memcpy((char *)cmp_value.c_str(), &data_base[start_pos+j], 8);
  //               if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
  //                 wrong_get++;
  //               }
  //               #endif
  //             #else
  //               auto ret = db->search(data_base[start_pos+j], value);
  //               #ifndef PERF_TEST
  //               if (!ret || (value != data_base[start_pos+j] && value != 0x19990627UL)) {
  //                 wrong_get++;
  //               }
  //               #endif
  //             #endif
              
  //             if(idx == 0 && (j + 1) % 100000 == 0) {
  //               std::cerr << "Operate: " << j + 1 << '\r'; 
  //             }
  //           }
  //         }
  //         if (wrong_get != 0) {
  //           std::cout << "thread " << global_thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
  //         }
  //       });
  //     }

  //     for (auto& t : threads)
  //       t.join();
          
  //     auto te = TIME_NOW;

  //     #ifdef STATISTIC_PMEM_INFO
  //     pin_end(&nvdimm_counter_end);
  //     print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
  //     #endif
      
  //     auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
  //     std::cout << "[Get]: Get " << GET_SIZE << ": " 
  //               << "cost " << use_seconds << "s, " 
  //               << "iops " << (double)(GET_SIZE)/use_seconds << " ." << std::endl;
  //   }
  // }

  // {
  //   // Put
  //   PUT_SIZE = 50000000;
  //   LOG_INFO(" @@@@@@@@@@@@@ put @@@@@@@@@@@@@@@");
  //   std::vector<std::thread> threads;
  //   std::atomic<uint64_t> thread_idx_count(0);
  //   size_t per_thread_size = PUT_SIZE / total_thread_num;

  //   #ifdef STATISTIC_PMEM_INFO
  //   pin_start(&nvdimm_counter_begin);
  //   #endif
    
  //   auto ts = TIME_NOW;

  //   for(int i = 0; i < thread_id_arr.size(); i++) {
  //     threads.emplace_back([&](){
  //       size_t idx = thread_idx_count.fetch_add(1); 
  //       global_thread_id = thread_id_arr[idx];
  //       bindCore(global_thread_id);
  //       size_t size = (idx == thread_id_arr.size()-1) ? (PUT_SIZE - idx*per_thread_size) : per_thread_size;
  //       size_t start_pos = idx * per_thread_size + 300000000;
  //       #ifdef VARVALUE
  //       std::string value(VALUE_LENGTH, '1');
  //       #endif
  //       for (size_t j = 0; j < size; ++j) {
  //         #ifdef VARVALUE
  //         #ifndef PERF_TEST
  //         memcpy((char *)value.c_str(), &data_base[start_pos+j], 8);
  //         #endif
  //         auto ret = db->insert(data_base[start_pos+j], value);
  //         #else
  //         auto ret = db->insert(data_base[start_pos+j], data_base[start_pos+j]);
  //         #endif
  //         // if (ret != 1) {
  //         //     std::cout << "Put error, key: " << data_base[start_pos+j] << ", size: " << j << std::endl;
  //         //     assert(0);
  //         // }
  //         if(idx == 0 && (j + 1) % 100000 == 0) {
  //           std::cerr << "Operate: " << j + 1 << '\r'; 
  //         }
  //       }
  //     });
  //   }

  //   for (auto& t : threads)
  //     t.join();
        
  //   auto te = TIME_NOW;

  //   #ifdef STATISTIC_PMEM_INFO
  //   pin_end(&nvdimm_counter_end);
  //   print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
  //   #endif

  //   auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
  //   std::cout << "[Put]: Put " << PUT_SIZE << ": " 
  //             << "cost " << use_seconds << "s, " 
  //             << "iops " << (double)(PUT_SIZE)/use_seconds << " ." << std::endl;
  // }
  delete db;
#endif
#endif
  return 0;
}