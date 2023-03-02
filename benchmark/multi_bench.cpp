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

// #define STATISTIC_PMEM_INFO 
#define PERF_TEST
#define USE_BULKLOAD
#define ZIPFAN_TEST
#define MIX_TEST
#ifdef STASTISTIC_NALI_CDF
size_t nali::test_total_keys = 0;
#endif

using KEY_TYPE = size_t;

#ifdef VARVALUE
using VALUE_TYPE = std::string;
constexpr size_t VALUE_LENGTH = 128;
#else
using VALUE_TYPE = size_t;
#endif

int8_t global_numa_map[64];
thread_local size_t global_thread_id = -1;

std::string dataset_path = "/home/zzy/dataset/generate_random_ycsb.dat";

int numa0_thread_num = 16;
int numa1_thread_num = 16;
size_t BULKLOAD_SIZE = 0;
size_t LOAD_SIZE   = 200000000;
size_t PUT_SIZE    = 10000000;
size_t GET_SIZE    = 10000000;
size_t UPDATE_SIZE = 10000000;
size_t DELETE_SIZE = 10000000;
int Loads_type = 3;
size_t valuesize = 8;

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
    {"help",            no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int c;
  int opt_idx;
  std::string  dbName= "utree";
  std::string  load_file= "";
  int total_thread_num = 0;
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
          case 7: valuesize = atoi(optarg); break;
          case 8: show_help(argv[0]); return 0;
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

  // 测2,4,6->32线程
  numa0_thread_num = total_thread_num;
  numa1_thread_num = total_thread_num;
  total_thread_num *= 2;

  std::cout << "NUMA0 THREAD NUMBER:   " << numa0_thread_num << std::endl;
  std::cout << "NUMA1 THREAD NUMBER:   " << numa1_thread_num << std::endl;
  std::cout << "LOAD_SIZE:             " << LOAD_SIZE << std::endl;
  std::cout << "PUT_SIZE:              " << PUT_SIZE << std::endl;
  std::cout << "GET_SIZE:              " << GET_SIZE << std::endl;
  std::cout << "UPDATE_SIZE:           " << UPDATE_SIZE << std::endl;
  std::cout << "DB  name:              " << dbName << std::endl;
  std::cout << "Workload:              " << load_file << std::endl;

  if (Loads_type == 5 && dbName == "pactree") {
    return 0;
  }
  
  if (Loads_type == 3 && dbName == "alexol") {
    return 0;
  }

  std::vector<uint64_t> data_base;
  switch (Loads_type)
  {
    case 3:
      data_base = load_data_from_osm<uint64_t>("/home/zzy/dataset/generate_random_ycsb.dat");;
      break;
    case 4:
      data_base = load_data_from_osm<uint64_t>("/home/zzy/dataset/generate_random_osm_longlat.dat");;
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

  size_t init_dram_space_use = physical_memory_used_by_process();
  std::cout << "before newdb, dram space use: " << init_dram_space_use / 1024.0 /1024.0  << " GB" << std::endl;

  LOG_INFO("@@@@@@@@@@@@ Init @@@@@@@@@@@@");

  Tree<size_t, uint64_t> *real_db = nullptr;

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

  Tree<KEY_TYPE, VALUE_TYPE> *db = nullptr;
  if  (dbName == "alexol") {
    real_db = new nali::alexoldb<size_t, uint64_t>();
    db = new nali::logdb<KEY_TYPE, VALUE_TYPE>(real_db, thread_ids);
  }
  // else if (dbName == "nali") {
  //   real_db = new nali::nalidb<size_t, uint64_t>();
  //   db = new nali::logdb<KEY_TYPE, VALUE_TYPE>(real_db, thread_ids);
  // } 
  else if (dbName == "utree") {
    real_db = new nali::utree_db<size_t, uint64_t>();
    db = real_db;
  } 
  // else if (dbName == "pactree") {
  //   real_db = new nali::pactree_db<size_t, uint64_t>(2);
  //   db = real_db;
  // } 
  // else if (dbName == "lbtree") {
  //   real_db = new nali::lbtree_db<size_t, uint64_t>(total_thread_num);
  //   db = real_db;
  // } 
  else if (dbName == "dptree") {
    real_db = new nali::dptree_db<size_t, uint64_t>(total_thread_num);
    db = real_db;
  } 
  else if (dbName == "nap") {
    real_db = new nali::napfastfair_db<size_t, uint64_t>();
    db = real_db;
  } else {
    LOG_INFO("not defined db: %s", dbName.c_str());
    assert(false);
  }
  assert(real_db);

  // Tree<KEY_TYPE, VALUE_TYPE> *db = real_db;

  #ifdef USE_BULKLOAD
  // alexol/apex must bulkload 10M/1M sorted kv
  {
    #ifdef STATISTIC_PMEM_INFO
    pin_start(&nvdimm_counter_begin);
    #endif
    BULKLOAD_SIZE = 10000000;
    auto values = new std::pair<uint64_t, uint64_t>[BULKLOAD_SIZE];
    for (int i = 0; i < BULKLOAD_SIZE; i++) {
      values[i].first = data_base[i];
      values[i].second = data_base[i];
    }

    if (dbName == "alexol" || dbName == "apex") {
      std::sort(values, values + BULKLOAD_SIZE,
        [](auto const& a, auto const& b) { return a.first < b.first; });
    }

    LOG_INFO("@@@@ BULK LOAD START @@@@");
    global_thread_id = 0; // thread0 do bulkload
    nali::bindCore(global_thread_id);
    db->bulk_load(values, BULKLOAD_SIZE);
    LOG_INFO("@@@@ BULK LOAD END @@@@");
    #ifdef STATISTIC_PMEM_INFO
    pin_end(&nvdimm_counter_end);
    print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif
  }
  #endif

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
    std::atomic_int thread_idx_count(0);
    size_t per_thread_size = LOAD_SIZE / total_thread_num;
    for(int i = 0; i < thread_id_arr.size(); i++) {
      threads.emplace_back([&](){
        int idx = thread_idx_count.fetch_add(1); 
        global_thread_id = thread_id_arr[idx];
        nali::bindCore(global_thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (LOAD_SIZE-idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size + BULKLOAD_SIZE;
        #ifdef VARVALUE
        std::string value(VALUE_LENGTH, '1');
        #endif
        for (size_t j = 0; j < size; ++j) {
          #ifdef VARVALUE
          memcpy((char *)value.c_str(), &data_base[start_pos+j], 8);
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
    print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif

    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Load]: Load " << LOAD_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(LOAD_SIZE)/(double)use_seconds << " ." << std::endl;
    std::cout << "dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 /1024.0  << " GB" << std::endl;
  }

  #ifdef STASTISTIC_NALI_CDF
  db->get_info();
  exit(0);
  #endif

  {
    // Put
    LOG_INFO(" @@@@@@@@@@@@@ put @@@@@@@@@@@@@@@");
    std::vector<std::thread> threads;
    std::atomic_int thread_idx_count(0);
    size_t per_thread_size = PUT_SIZE / total_thread_num;

    #ifdef STATISTIC_PMEM_INFO
    pin_start(&nvdimm_counter_begin);
    #endif
    
    auto ts = TIME_NOW;

    for(int i = 0; i < thread_id_arr.size(); i++) {
      threads.emplace_back([&](){
        int idx = thread_idx_count.fetch_add(1); 
        global_thread_id = thread_id_arr[idx];
        nali::bindCore(global_thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (PUT_SIZE - idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size + LOAD_SIZE + BULKLOAD_SIZE;
        #ifdef VARVALUE
        std::string value(VALUE_LENGTH, '1');
        #endif
        for (size_t j = 0; j < size; ++j) {
          #ifdef VARVALUE
          memcpy((char *)value.c_str(), &data_base[start_pos+j], 8);
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
    pin_end(&nvdimm_counter_end);
    print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif

    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Put]: Put " << PUT_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(PUT_SIZE)/use_seconds << " ." << std::endl;
  }

  {
    // Get
    Random get_rnd(0, LOAD_SIZE+PUT_SIZE+BULKLOAD_SIZE-1);
    for (size_t i = 0; i < GET_SIZE; ++i) {
      int idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }

    LOG_INFO(" @@@@@@@@@@@@@ get @@@@@@@@@@@@@@@");

    for (int loop = 0; loop < 1; loop++) {
      std::vector<std::thread> threads;
      std::atomic_int thread_idx_count(0);
      size_t per_thread_size = GET_SIZE / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          int idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          nali::bindCore(global_thread_id);
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
                memcpy((char *)cmp_value.c_str(), &data_base[start_pos+j], 8);
                auto ret = db->search(data_base[start_pos+j], value);
                #ifndef PERF_TEST
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
    }
  }

  {
    // update
    Random get_rnd(0, LOAD_SIZE+PUT_SIZE+BULKLOAD_SIZE-1);
    
    for (size_t i = 0; i < UPDATE_SIZE; ++i) {
      int idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }

    LOG_INFO(" @@@@@@@@@@@@@ update @@@@@@@@@@@@@@@");

    for (int loop = 0; loop < 1; loop++) {
      std::vector<std::thread> threads;
      std::atomic_int thread_idx_count(0);
      size_t per_thread_size = UPDATE_SIZE / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          int idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          nali::bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (UPDATE_SIZE-idx*per_thread_size) : per_thread_size;
          size_t start_pos = idx * per_thread_size;
              
          int wrong_get = 0;
          #ifdef VARVALUE
            std::string value;
            std::string cmp_value(VALUE_LENGTH, '1');
          #else
            size_t value;
          #endif
          size_t t_v = 0x19990627UL;
          for (size_t j = 0; j < size; ++j) {
            #ifdef VARVALUE
              memcpy((char *)value.c_str(), &t_v, 8);
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
      print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Update]: Update " << UPDATE_SIZE << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(UPDATE_SIZE)/use_seconds << " ." << std::endl;
    }
  }

#ifdef ZIPFAN_TEST
  // zipfan read test
  {
    size_t total_zipfan_ops = 10000000;
    std::vector<float> zipfan_params = {0.99, 0.9, 0.8, 0.7, 0.6, 0.5};
    float theta = 0.99;
    bool is_read = true;
zipfan_test:
    LOG_INFO(" @@@@@@@@@@@@@ zipfan update/get @@@@@@@@@@@@@@@");
    for (int loop = 0; loop < zipfan_params.size(); loop++) {
      theta = zipfan_params[loop];
      std::vector<std::thread> threads;
      std::atomic_int thread_idx_count(0);
      size_t per_thread_size = total_zipfan_ops / total_thread_num;
      // std::cout << "generate test data begin" << std::endl;
      size_t *zipfan_base = get_search_keys_zipf_with_theta<uint64_t>(data_base.data()+20000000, 8000000, total_zipfan_ops, theta); // 每次zipfan测试的数据集应该不一样才好
      // std::cout << "generate test data end" << std::endl;
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          int idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          nali::bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (total_zipfan_ops-idx*per_thread_size) : per_thread_size;
          size_t start_pos = idx * per_thread_size;
              
          int wrong_get = 0;
          #ifdef VARVALUE
            std::string value(VALUE_LENGTH, '1');
            std::string cmp_value(VALUE_LENGTH, '1');
          #else
            size_t value;
          #endif
          size_t t_v = 0x19990627UL;
          for (size_t j = 0; j < size; ++j) {
            bool ret;
            if (!is_read) {
              #ifdef VARVALUE
                memcpy((char *)value.c_str(), &t_v, 8);
                ret = db->update(zipfan_base[start_pos+j], value);
              #else
                ret = db->update(zipfan_base[start_pos+j], t_v);
              #endif
            } else {
              #ifdef VARVALUE
                memcpy((char *)cmp_value.c_str(), &zipfan_base[start_pos+j], 8);
                ret = db->search(zipfan_base[start_pos+j], value);
                #ifndef PERF_TEST
                if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
                  wrong_get++;
                }
                #endif
              #else
                ret = db->search(zipfan_base[start_pos+j], value);
                #ifndef PERF_TEST
                if (!ret || (value != zipfan_base[start_pos+j] && value != 0x19990627UL)) {
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
      print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::string test_type = is_read ? "[zipfan_read]: " : "[zipfan_update]: ";
      std::cout << test_type << "zipfan" << theta << " " << total_zipfan_ops << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(total_zipfan_ops)/use_seconds << " ." << std::endl;
      delete [] zipfan_base;
    }
    if (is_read) {
      is_read = false;
      goto zipfan_test;
    }
  }
#endif

#ifdef MIX_TEST
    {
    size_t total_mix_ops = 10000000;
    std::vector<float> insert_ratios = {0.05, 0.2, 0.5, 0.8, 0.95};
    // std::vector<float> insert_ratios = {0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0};
    float insert_ratio = 0;

    LOG_INFO(" @@@@@@@@@@@@@ mixed update/get @@@@@@@@@@@@@@@");

    for (int loop = 0; loop < insert_ratios.size(); loop++) {
      // mix update/get
      util::FastRandom ranny(loop+1999);
      std::vector<double> random_ratio(total_mix_ops);
      Random get_rnd(0, LOAD_SIZE+PUT_SIZE-1);
      for (size_t i = 0; i < total_mix_ops; ++i) {
        int idx = get_rnd.Next();
        std::swap(data_base[i], data_base[idx]);
        random_ratio[i] = ranny.ScaleFactor();
      }

      insert_ratio = insert_ratios[loop];
      std::vector<std::thread> threads;
      std::atomic_int thread_idx_count(0);
      size_t per_thread_size = total_mix_ops / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          int idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          nali::bindCore(global_thread_id);
          size_t size = (idx == thread_id_arr.size()-1) ? (total_mix_ops-idx*per_thread_size) : per_thread_size;
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
              if (random_ratio[start_pos+j] < insert_ratio) {
                #ifdef VARVALUE
                  memcpy((char *)value.c_str(), &t_v, 8);
                  ret = db->update(data_base[start_pos+j], value);
                #else
                  ret = db->update(data_base[start_pos+j], t_v);
                #endif
              } else {
                #ifdef VARVALUE
                  memcpy((char *)cmp_value.c_str(), &data_base[start_pos+j], 8);
                  ret = db->search(data_base[start_pos+j], value);
                  #ifndef PERF_TEST
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
      print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Mix]: Mix" << insert_ratio << " " << total_mix_ops << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(total_mix_ops)/use_seconds << " ." << std::endl;
    }
  }
#endif
  
  {
    // Scan
    const uint64_t total_scan_ops = 1000000;
    std::vector<int> scan_size = {100};
    
    Random get_rnd(0, LOAD_SIZE+PUT_SIZE-10000);
    for (size_t i = 0; i < total_scan_ops; ++i) {
      int idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }

    LOG_INFO(" @@@@@@@@@@@@@ scan @@@@@@@@@@@@@@@");

    for (int scan : scan_size) {
      std::vector<std::thread> threads;
      std::atomic_int thread_idx_count(0);
      size_t per_thread_size = total_scan_ops / total_thread_num;
      
      #ifdef STATISTIC_PMEM_INFO
      pin_start(&nvdimm_counter_begin);
      #endif

      auto ts = TIME_NOW;
      for (int i = 0; i < thread_id_arr.size(); ++i) {
          threads.emplace_back([&](){
          int idx = thread_idx_count.fetch_add(1); 
          global_thread_id = thread_id_arr[idx];
          nali::bindCore(global_thread_id);
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
      print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
      #endif
      
      auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
      std::cout << "[Scan]: Scan " << total_scan_ops << ": per scan size " << scan << ": " 
                << "cost " << use_seconds << "s, " 
                << "iops " << (double)(total_scan_ops)/use_seconds << " ." << std::endl;
    }
  }

  {
     // Delete
    Random get_rnd(0, LOAD_SIZE+PUT_SIZE-1);
    for (size_t i = 0; i < DELETE_SIZE; ++i) {
      int idx = get_rnd.Next();
      std::swap(data_base[i], data_base[idx]);
    }

    LOG_INFO(" @@@@@@@@@@@@@ delete @@@@@@@@@@@@@@@");

    std::vector<std::thread> threads;
    std::atomic_int thread_idx_count(0);
    size_t per_thread_size = DELETE_SIZE / total_thread_num;
    
    #ifdef STATISTIC_PMEM_INFO
    pin_start(&nvdimm_counter_begin);
    #endif

    auto ts = TIME_NOW;
    for (int i = 0; i < thread_id_arr.size(); ++i) {
        threads.emplace_back([&](){
        int idx = thread_idx_count.fetch_add(1); 
        global_thread_id = thread_id_arr[idx];
        nali::bindCore(global_thread_id);
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
    print_counter_change(nvdimm_counter_begin, nvdimm_counter_end);
    #endif
    
    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Delete]: Delete " << DELETE_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(DELETE_SIZE)/use_seconds << " ." << std::endl;
  }

  delete db;

  return 0;
}