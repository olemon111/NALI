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
#include "util/logging.h"
#include "logdb.h"
#include "db_interface.h"
#include "util/sosd_util.h"

// #define STATISTIC_PMEM_INFO 
// #define USE_BULKLOAD

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

namespace nali {

thread_local size_t thread_id = -1;
int8_t numa_map[max_thread_num];

#ifdef STASTISTIC_NALI_CDF
size_t test_total_keys = 0;
#endif

}

using KEY_TYPE = size_t;

#ifdef VARVALUE
using VALUE_TYPE = std::string;
constexpr size_t VALUE_LENGTH = 128;
#else
using VALUE_TYPE = size_t;
#endif

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

std::string dataset_path = "/home/zzy/dataset/generate_random_ycsb.dat";

int numa0_thread_num = 16;
int numa1_thread_num = 16;
size_t LOAD_SIZE   = 390000000;
size_t PUT_SIZE    = 10000000;
size_t GET_SIZE    = 100000000;
size_t DELETE_SIZE = 100000000;
int Loads_type = 3;
size_t valuesize = 8;

template<typename T>
std::vector<T>load_data_from_osm(const std::string dataname)
{
  return util::load_data<T>(dataname);
}

int main(int argc, char *argv[]) {
    static struct option opts[] = {
  /* NAME               HAS_ARG            FLAG  SHORTNAME*/
    // {"thread",          required_argument, NULL, 't'},
    {"numa0-thread",    required_argument, NULL, 0},
    {"numa1-thread",    required_argument, NULL, 0},
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
  std::string  dbName= "nali";
  std::string  load_file= "";
  while ((c = getopt_long(argc, argv, "s:dh", opts, &opt_idx)) != -1) {
    switch (c) {
      case 0:
        switch (opt_idx) {
          case 0: numa0_thread_num = atoi(optarg); break;
          case 1: numa1_thread_num = atoi(optarg); break;
          case 2: LOAD_SIZE = atoi(optarg); break;
          case 3: PUT_SIZE = atoi(optarg); break;
          case 4: GET_SIZE = atoi(optarg); break;
          case 5: dbName = optarg; break;
          case 6: load_file = optarg; break;
          case 7: Loads_type = atoi(optarg); break;
          case 8: valuesize = atoi(optarg); break;
          case 9: show_help(argv[0]); return 0;
          default: std::cerr << "Parse Argument Error!" << std::endl; abort();
        }
        break;
      case 'h': show_help(argv[0]); return 0;
      case '?': break;
      default:  std::cout << (char)c << std::endl; abort();
    }
  }

  // if (valuesize == 8) {
  //   #undef VARVALUE
  // }

  // numa0_thread_num = 0;
  // numa1_thread_num = 1;
  // LOAD_SIZE = 10000000;
  // PUT_SIZE = 10000000;
  // GET_SIZE = 10000000;

  std::cout << "NUMA0 THREAD NUMBER:   " << numa0_thread_num << std::endl;
  std::cout << "NUMA1 THREAD NUMBER:   " << numa1_thread_num << std::endl;
  std::cout << "LOAD_SIZE:             " << LOAD_SIZE << std::endl;
  std::cout << "PUT_SIZE:              " << PUT_SIZE << std::endl;
  std::cout << "GET_SIZE:              " << GET_SIZE << std::endl;
  std::cout << "DB  name:              " << dbName << std::endl;
  std::cout << "Workload:              " << load_file << std::endl;

  const int total_thread_num = numa0_thread_num + numa1_thread_num;

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

  if  (dbName == "alexol") {
    // real_db = new nali::alexoldb<size_t, uint64_t>();
  } else if (dbName == "fastfair") {
    // real_db = new nali::fastfairdb<size_t, uint64_t>();
  } else if (dbName == "nali") {
      real_db = new nali::nalidb<size_t, uint64_t>();
  } else if (dbName == "art") {
    // real_db = new nali::artdb<size_t, uint64_t>();
  } else {
    LOG_INFO("not defined db: %s", dbName.c_str());
    assert(false);
  }
  assert(real_db);

  // generate thread_ids
  assert(numa0_thread_num >= 0 && numa0_thread_num <=16);
  assert(numa1_thread_num >= 0 && numa1_thread_num <=16);
  std::vector<std::vector<int>> thread_ids(nali::numa_max_node, vector<int>());
  std::vector<int> thread_id_arr;
  for (int i = 0; i < numa0_thread_num; i++) {
    thread_ids[0].push_back(i);
    thread_id_arr.push_back(i);
  }
  for (int i = 0; i < numa1_thread_num; i++) {
    thread_ids[1].push_back(16+i);
    thread_id_arr.push_back(16+i);
  }

  Tree<KEY_TYPE, VALUE_TYPE> *db = new nali::logdb<KEY_TYPE, VALUE_TYPE>(real_db, thread_ids);
  // Tree<KEY_TYPE, VALUE_TYPE> *db = real_db;

  #ifdef USE_BULKLOAD
  // alexol must bulkload 10M/1M sorted kv
  {
    #ifdef STATISTIC_PMEM_INFO
    pin_start(&nvdimm_counter_begin);
    #endif
    int init_size = 10000000;
    auto values = new std::pair<uint64_t, uint64_t>[init_size];
    // size_t start_idx = LOAD_SIZE + PUT_SIZE; // TODO: if has mixed test, need add mix put size
    size_t start_idx = 0;
    for (int i = 0; i < init_size; i++) {
      values[i].first = data_base[i+start_idx];
      values[i].second = data_base[i+start_idx];
    }
    if (dbName != "nali") {
      std::sort(values, values + init_size,
        [](auto const& a, auto const& b) { return a.first < b.first; });
    }

    LOG_INFO("@@@@ BULK LOAD START @@@@");
    nali::thread_id = 0; // thread0 do bulkload
    nali::bindCore(nali::thread_id);
    db->bulk_load(values, init_size);
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
        nali::thread_id = thread_id_arr[idx];
        nali::bindCore(nali::thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (LOAD_SIZE-idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size;
        #ifdef VARVALUE
        std::string value(VALUE_LENGTH, '1');
        #endif
        for (size_t j = 0; j < size; ++j) {
          // std::cerr << "insert times: " << j  << "\n";
          #ifdef VARVALUE
          memcpy((char *)value.c_str(), &data_base[start_pos+j], 8);
          bool ret = db->insert(data_base[start_pos+j], value);
          #else
          bool ret = db->insert(data_base[start_pos+j], data_base[start_pos+j]);
          #endif
          // if (!ret) {
          //     std::cout << "load error, key: " << data_base[start_pos+j] << ", pos: " << j << std::endl;
          //     assert(false);
          // }

          if(idx == 0 && (j + 1) % 10000000 == 0) {
            std::cout << "Operate: " << j + 1 << std::endl;  
            std::cout << "dram space use: " << (physical_memory_used_by_process() - init_dram_space_use) / 1024.0 /1024.0  << " GB" << std::endl;
            // db->get_info();
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
        nali::thread_id = thread_id_arr[idx];
        nali::bindCore(nali::thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (PUT_SIZE - idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size + LOAD_SIZE;
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
    Random get_rnd(0, LOAD_SIZE+PUT_SIZE-1);
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
          nali::thread_id = thread_id_arr[idx];
          nali::bindCore(nali::thread_id);
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
                if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
                  wrong_get++;
                }
              #else
                auto ret = db->search(data_base[start_pos+j], value);
                if (!ret || (value != data_base[start_pos+j] && value != 0x19990627UL)) {
                  wrong_get++;
                }
              #endif
              
              if(idx == 0 && (j + 1) % 100000 == 0) {
                std::cerr << "Operate: " << j + 1 << '\r'; 
              }
            }
          }
          if (wrong_get != 0) {
            std::cout << "thread " << nali::thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
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

  // {
  //   size_t total_mix_ops = 10000000;
  //   std::vector<float> insert_ratios = {1};
  //   // std::vector<float> insert_ratios = {0,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,1.0};
  //   float insert_ratio = 0;

  //   LOG_INFO(" @@@@@@@@@@@@@ mixed update/get @@@@@@@@@@@@@@@");

  //   for (int loop = 0; loop < insert_ratios.size(); loop++) {
  //     // mix update/get
  //     util::FastRandom ranny(18);
  //     std::vector<double> random_ratio(total_mix_ops);
  //     Random get_rnd(0, LOAD_SIZE+PUT_SIZE-1);
  //     for (size_t i = 0; i < total_mix_ops; ++i) {
  //       int idx = get_rnd.Next();
  //       std::swap(data_base[i], data_base[idx]);
  //       random_ratio[i] = ranny.ScaleFactor();
  //     }

  //     insert_ratio = insert_ratios[loop];
  //     std::vector<std::thread> threads;
  //     std::atomic_int thread_idx_count(0);
  //     size_t per_thread_size = total_mix_ops / total_thread_num;
      
  //     #ifdef STATISTIC_PMEM_INFO
  //     pin_start(&nvdimm_counter_begin);
  //     #endif

  //     auto ts = TIME_NOW;
  //     for (int i = 0; i < thread_id_arr.size(); ++i) {
  //         threads.emplace_back([&](){
  //         int idx = thread_idx_count.fetch_add(1); 
  //         nali::thread_id = thread_id_arr[idx];
  //         nali::bindCore(nali::thread_id);
  //         size_t size = (idx == thread_id_arr.size()-1) ? (total_mix_ops-idx*per_thread_size) : per_thread_size;
  //         size_t start_pos = idx * per_thread_size;
              
  //         int wrong_get = 0;
  //         #ifdef VARVALUE
  //           std::string value(VALUE_LENGTH, '1');
  //           std::string cmp_value(VALUE_LENGTH, '1');
  //         #else
  //           size_t value;
  //         #endif
  //         size_t t_v = 0x19990627UL;
  //         for (int t = 0; t < 1; t++) {
  //           for (size_t j = 0; j < size; ++j) {
  //             bool ret;
  //             if (random_ratio[start_pos+j] < insert_ratio) {
  //               #ifdef VARVALUE
  //                 memcpy((char *)value.c_str(), &t_v, 8);
  //                 ret = db->update(data_base[start_pos+j], value);
  //               #else
  //                 ret = db->update(data_base[start_pos+j], t_v);
  //               #endif
  //             } else {
  //               #ifdef VARVALUE
  //                 memcpy((char *)cmp_value.c_str(), &data_base[start_pos+j], 8);
  //                 ret = db->search(data_base[start_pos+j], value);
  //                 if (!ret || (value != cmp_value && value != std::to_string(0x19990627UL))) {
  //                   wrong_get++;
  //                 }
  //               #else
  //                 ret = db->search(data_base[start_pos+j], value);
  //                 if (!ret || (value != data_base[start_pos+j] && value != 0x19990627UL)) {
  //                   wrong_get++;
  //                 }
  //               #endif
  //             }
              
  //             if(idx == 0 && (j + 1) % 100000 == 0) {
  //               std::cerr << "Operate: " << j + 1 << '\r'; 
  //             }
  //           }
  //         }
  //         if (wrong_get != 0) {
  //           std::cout << "thread " << nali::thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
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
  //     std::cout << "[Mix]: Mix " << total_mix_ops << ": " 
  //               << "cost " << use_seconds << "s, " 
  //               << "iops " << (double)(total_mix_ops)/use_seconds << " ." << std::endl;
  //   }
  // }
  
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
          nali::thread_id = thread_id_arr[idx];
          nali::bindCore(nali::thread_id);
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
              if(idx == 0 && (j + 1) % 100000 == 0) {
                std::cerr << "Operate: " << j + 1 << '\r'; 
              }
            }
          }
          if (wrong_get != 0) {
            std::cout << "thread " << nali::thread_id << ", total scan kvs: " << total_get << ", wrong get: " << wrong_get << std::endl;
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
      std::cout << "[Scan]: Scan " << total_scan_ops << " , per scan size: " << scan << ": " 
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
        nali::thread_id = thread_id_arr[idx];
        nali::bindCore(nali::thread_id);
        size_t size = (idx == thread_id_arr.size()-1) ? (DELETE_SIZE-idx*per_thread_size) : per_thread_size;
        size_t start_pos = idx * per_thread_size;
            
        int wrong_get = 0;
        for (int t = 0; t < 1; t++) {
          for (size_t j = 0; j < size; ++j) {
            bool ret;
            size_t value;
            ret = db->erase(data_base[start_pos+j]);
            
            // if (!ret || value != data_base[start_pos+j]) {
            //   // std::cout << "Get error!" << std::endl;
            //   wrong_get++;
            // }
            if(idx == 0 && (j + 1) % 100000 == 0) {
              std::cerr << "Operate: " << j + 1 << '\r'; 
            }
          }
        }
        if (wrong_get != 0) {
          std::cout << "thread " << nali::thread_id << ", total get: " << size << ", wrong get: " << wrong_get << std::endl;
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