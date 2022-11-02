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

#include "utils.h"
#include "util/logging.h"
#include "db_interface.h"
#include "util/sosd_util.h"

#define TIME_NOW (std::chrono::high_resolution_clock::now())

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

int thread_num = 1;
size_t LOAD_SIZE   = 10000000;
size_t PUT_SIZE    = 6000000;
size_t GET_SIZE    = 1000000;
size_t DELETE_SIZE = 1000000;
int Loads_type = 0;

template<typename T>
std::vector<T>load_data_from_osm(const std::string dataname)
{
  return util::load_data<T>(dataname);
}

int main(int argc, char *argv[]) {
    static struct option opts[] = {
  /* NAME               HAS_ARG            FLAG  SHORTNAME*/
    {"thread",          required_argument, NULL, 't'},
    {"load-size",       required_argument, NULL, 0},
    {"put-size",        required_argument, NULL, 0},
    {"get-size",        required_argument, NULL, 0},
    {"dbname",          required_argument, NULL, 0},
    {"workload",        required_argument, NULL, 0},
    {"loadstype",       required_argument, NULL, 0},
    {"help",            no_argument,       NULL, 'h'},
    {NULL, 0, NULL, 0}
  };

  int c;
  int opt_idx;
  std::string  dbName= "alex";
  std::string  load_file= "";
  while ((c = getopt_long(argc, argv, "t:s:dh", opts, &opt_idx)) != -1) {
    switch (c) {
      case 0:
        switch (opt_idx) {
          case 0: thread_num = atoi(optarg); break;
          case 1: LOAD_SIZE = atoi(optarg); break;
          case 2: PUT_SIZE = atoi(optarg); break;
          case 3: GET_SIZE = atoi(optarg); break;
          case 4: dbName = optarg; break;
          case 5: load_file = optarg; break;
          case 6: Loads_type = atoi(optarg); break;
          case 7: show_help(argv[0]); return 0;
          default: std::cerr << "Parse Argument Error!" << std::endl; abort();
        }
        break;
      case 't': thread_num = atoi(optarg); break;
      case 'h': show_help(argv[0]); return 0;
      case '?': break;
      default:  std::cout << (char)c << std::endl; abort();
    }
  }

  std::cout << "THREAD NUMBER:         " << thread_num << std::endl;
  std::cout << "LOAD_SIZE:             " << LOAD_SIZE << std::endl;
  std::cout << "PUT_SIZE:              " << PUT_SIZE << std::endl;
  std::cout << "GET_SIZE:              " << GET_SIZE << std::endl;
  std::cout << "DB  name:              " << dbName << std::endl;
  std::cout << "Workload:              " << load_file << std::endl;

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
      LOG_ERROR("not defined loads_type");
      assert(false);
      break;
  }
  
  LOG_INFO("@@@@@@@@@@@@ Init @@@@@@@@@@@@");

  Tree<size_t, size_t> *db = nullptr;
  if (dbName == "alex") {
    db = new nali::alexdb<size_t, size_t>();
  } else if (dbName == "fastfair") {
    db = new nali::fastfairdb<size_t, size_t>();
  } else {
    LOG_ERROR("not defined db: %s", dbName.c_str());
    assert(false);
  }
   

  if (dbName == "alex") {
    int init_size = 10000000;
    std::mt19937_64 gen_payload(std::random_device{}());
    auto values = new std::pair<uint64_t, uint64_t>[init_size];
    for (int i = 0; i < init_size; i++) {
      // values[i].first = data_base[data_base.size() - i - 1];
      values[i].first = data_base[i];
      values[i].second = static_cast<uint64_t>(gen_payload());
    }
    std::sort(values, values + init_size,
              [](auto const& a, auto const& b) { return a.first < b.first; });
    LOG_INFO("@@@@ ALEX BULK LOAD START @@@@");
    db->bulk_load(values, init_size);
    LOG_INFO("@@@@ ALEX BULK LOAD END @@@@");
  }

  {
    LOG_INFO(" @@@@@@@@@@@@@ LOAD @@@@@@@@@@@@@@@");
    // Load
    auto ts = TIME_NOW;
    std::vector<std::thread> threads;
    std::atomic_int thread_id_count(0);
    size_t per_thread_size = LOAD_SIZE / thread_num;
    for(int i = 0; i < thread_num; i ++) {
        threads.emplace_back([&](){
            int thread_id = thread_id_count.fetch_add(1);
            size_t start_pos = thread_id * per_thread_size;
            size_t size = (thread_id == thread_num-1) ? LOAD_SIZE-(thread_num-1)*per_thread_size : per_thread_size;
            for (size_t j = 0; j < size; ++j) {
                bool ret = db->insert(data_base[start_pos+j], data_base[start_pos+j]);
                if (!ret) {
                    std::cout << "load error, key: " << data_base[start_pos+j] << ", pos: " << j << std::endl;
                    assert(false);
                }

                if(thread_id == 0 && (j + 1) % 1000000 == 0) std::cerr << "Operate: " << j + 1 << '\r';  
            }
        });
    }
    for (auto& t : threads)
        t.join();

    auto te = TIME_NOW;
    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Load]: Load " << LOAD_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(LOAD_SIZE)/(double)use_seconds << " ." << std::endl;
  }

  {
     // Put
    LOG_INFO(" @@@@@@@@@@@@@ put @@@@@@@@@@@@@@@");
    std::vector<std::thread> threads;
    std::atomic_int thread_id_count(0);
    size_t per_thread_size = PUT_SIZE / thread_num;
    auto ts = TIME_NOW;
    for(int i = 0; i < thread_num; i ++) {
        threads.emplace_back([&](){
            int thread_id = thread_id_count.fetch_add(1);
            size_t start_pos = thread_id * per_thread_size + LOAD_SIZE;
            size_t size = (thread_id == thread_num-1) ? PUT_SIZE-(thread_num-1)*per_thread_size : per_thread_size;
            for (size_t j = 0; j < size; ++j) {
                auto ret = db->insert(data_base[start_pos+j], data_base[start_pos+j]);
                if (ret != 1) {
                    std::cout << "Put error, key: " << data_base[start_pos+j] << ", size: " << j << std::endl;
                    assert(0);
                }
                if(thread_id == 0 && (j + 1) % 100000 == 0) std::cerr << "Operate: " << j + 1 << '\r'; 
            }
        });
    }
    for (auto& t : threads)
        t.join();
        
    auto te = TIME_NOW;
    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Put]: Put " << PUT_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(PUT_SIZE)/use_seconds << " ." << std::endl;
  }

  {
     // Get
    LOG_INFO(" @@@@@@@@@@@@@ get @@@@@@@@@@@@@@@");
    std::vector<std::thread> threads;
    std::atomic_int thread_id_count(0);
    size_t per_thread_size = GET_SIZE / thread_num;
    Random get_rnd(0, GET_SIZE-1);
    for (size_t i = 0; i < GET_SIZE; ++i)
        std::swap(data_base[i],data_base[get_rnd.Next()]);
    auto ts = TIME_NOW;
    for (int i = 0; i < thread_num; ++i) {
        threads.emplace_back([&](){
            int thread_id = thread_id_count.fetch_add(1);
            size_t start_pos = thread_id *per_thread_size;
            size_t size = (thread_id == thread_num-1) ? GET_SIZE-(thread_num-1)*per_thread_size : per_thread_size;
            size_t value;
            for (size_t j = 0; j < size; ++j) {
                bool ret = db->search(data_base[start_pos+j], &value);
                if (!ret || value != data_base[start_pos+j]) {
                    std::cout << "Get error!" << std::endl;
                }
                if(thread_id == 0 && (j + 1) % 100000 == 0) std::cerr << "Operate: " << j + 1 << '\r'; 
            }
        });
    }
    for (auto& t : threads)
        t.join();
        
    auto te = TIME_NOW;
    auto use_seconds = std::chrono::duration_cast<std::chrono::microseconds>(te - ts).count() * 1.0 / 1000 / 1000;
    std::cout << "[Get]: Get " << GET_SIZE << ": " 
              << "cost " << use_seconds << "s, " 
              << "iops " << (double)(GET_SIZE)/use_seconds << " ." << std::endl;
  }

  // range

  // mix

  // delete

  delete db;

  return 0;
}