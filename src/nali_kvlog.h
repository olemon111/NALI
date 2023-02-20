#pragma once
#include <iostream>
#include <string.h>
#include <assert.h>
#include <libpmem.h>
#include <atomic>
#include <jemalloc/jemalloc.h>
#include "tree.h"
#include "nali_alloc.h"
#include "xxhash.h"
#include "rwlock.h"

extern thread_local size_t global_thread_id;

namespace nali {

// #define USE_PROXY

// #define VARVALUE

enum OP_t { INSERT, DELETE, UPDATE, TRASH }; // TRASH is for gc
using OP_VERSION = uint64_t;
constexpr int OP_BITS = 2;
constexpr int VERSION_BITS = sizeof(OP_VERSION) * 8 - OP_BITS;

constexpr uint64_t vlen_mask = (0xffffUL << 48);
constexpr uint64_t numa_id_mask = (0xffUL << 40);
constexpr uint64_t ppage_id_mask = (0xffUL << 32);
constexpr uint64_t log_offset_mask = 0xffffffffUL;
struct last_log_offest {
  uint16_t vlen_; // value_length
  uint8_t numa_id_; // last log's logfile numa id
  uint8_t ppage_id_; // last log's logfile number
  uint32_t log_offset_; // last log's in-logfile's offset
  last_log_offest() : numa_id_(UINT8_MAX) {}; // set to UINT8_MAX to indicate it is first log
  last_log_offest(uint16_t vlen, uint8_t numa_id, uint8_t ppage_id, uint32_t log_offset) : 
      vlen_(vlen), numa_id_(numa_id), ppage_id_(ppage_id), log_offset_(log_offset) {}
  last_log_offest(uint64_t addr_info) : vlen_((addr_info & vlen_mask)>>48), numa_id_((addr_info & numa_id_mask)>>40), 
      ppage_id_((addr_info & ppage_id_mask)>>32), log_offset_(addr_info & log_offset_mask) {}
  uint64_t to_uint64_t_offset() {
    return ((((uint64_t)vlen_) << 48) | (((uint64_t)numa_id_) << 40) | (((uint64_t)ppage_id_) << 32) | ((uint64_t)log_offset_));
  }
};

#pragma pack(1) //使范围内结构体按1字节方式对齐
template <typename KEY, typename VALUE>
class Pair_t {
 public:
  OP_VERSION op : OP_BITS;
  OP_VERSION version : VERSION_BITS;
  KEY _key;
  VALUE _value;
  union{
    last_log_offest _last_log_offset;
    uint64_t u_offset;
  };

  Pair_t() {}
  Pair_t(const KEY &k, const VALUE &v) {
    _key = k;
    _value = v;
  }
  Pair_t(const KEY &k) {
    _key = k;
  }

  KEY key() { return _key; }
  VALUE value() { return _value; }
  void load(char *p, bool load_value = true) {
    auto pt = reinterpret_cast<Pair_t<KEY, VALUE> *>(p);
    *this = *pt;
  }
  size_t klen() { return sizeof(KEY); }
  size_t vlen() { return sizeof(VALUE); }
  void set_key(KEY k) { _key = k; }
  void store_persist(void *addr) {
    auto p = reinterpret_cast<void *>(this);
    auto len = size();
    memcpy(addr, p, len);
    pmem_persist(addr, len);
  }
  constexpr size_t value_offset() { return sizeof(OP_VERSION) + sizeof(KEY); }
  void set_version(uint64_t old_version) { version = old_version + 1; }
  void set_op(OP_t o) { op = static_cast<uint16_t>(o); }
  // void set_last_log_offest(uint16_t numa_id, uint16_t ppage_id, uint32_t log_offset) { 
  //   _last_log_offset.numa_id_ = numa_id;
  //   _last_log_offset.ppage_id_ = ppage_id;
  //   _last_log_offset.log_offset_ = log_offset;
  // }
  void set_last_log_offest(uint64_t log_offset) {
    u_offset = log_offset;
  }

  OP_t get_op() { return static_cast<OP_t>(op); }
  void set_op_persist(OP_t o) {
    op = static_cast<uint16_t>(o);
    pmem_persist(reinterpret_cast<char *>(this), 8);
  }

  size_t size() {
    auto total_length = sizeof(OP_VERSION) + sizeof(KEY) + sizeof(VALUE) + sizeof(last_log_offest);
    return total_length;
  }
  uint64_t next_old_log() { return u_offset; }
};

// support var value
template<typename KEY>
class Pair_t<KEY, std::string> {
public:
  OP_VERSION op : OP_BITS;
  OP_VERSION version : VERSION_BITS;
  KEY _key;
  uint16_t _vlen;
  union{
    last_log_offest _last_log_offset;
    uint64_t u_offset;
  };
  std::string _value;

  // Pair_t(KEY k, char *v_ptr, size_t vlen) : _vlen(vlen) {
  //   _key = k;
  //   svalue.assign(v_ptr, _vlen);
  // }
  Pair_t() {}
  Pair_t(const KEY &k, const std::string &v) : _vlen(v.size()){
    _key = k;
    _value = v;
  }
  Pair_t(const KEY &k) : _vlen(0) {
    _key = k;
  }

  KEY key() { return _key; }
  std::string value() { return _value; }
  void load(char *p, bool load_value = true) {
    auto pt = reinterpret_cast<Pair_t *>(p);
    op = pt->op;
    _key = pt->_key;
    _vlen = pt->_vlen;
    if (load_value) {
      _value.assign(p + sizeof(OP_VERSION) + sizeof(KEY) + sizeof(uint16_t) + sizeof(uint64_t),
                    _vlen);
    }
  }
  size_t klen() { return sizeof(KEY); }
  size_t vlen() { return _vlen; }
  void set_key(KEY k) { _key = k; }
  void store_persist(char *addr) {
    auto p = reinterpret_cast<void *>(this);
    auto len = sizeof(OP_VERSION) + sizeof(KEY) + sizeof(uint16_t) + sizeof(uint64_t);
    memcpy(addr, p, len);
    if (likely(_vlen) > 0) {
      memcpy(addr+len, &_value[0], _vlen);
    }
    pmem_persist(addr, len + _vlen);
  }
  constexpr size_t value_offset() { return sizeof(OP_VERSION) + sizeof(KEY) + sizeof(uint16_t) + sizeof(uint64_t); }

  void set_version(uint64_t old_version) { version = old_version + 1; }
  void set_op(OP_t o) { op = static_cast<uint16_t>(o); }
  // void set_last_log_offest(uint16_t numa_id, uint16_t ppage_id, uint32_t log_offset) { 
  //   _last_log_offset.numa_id_ = numa_id;
  //   _last_log_offset.ppage_id_ = ppage_id;
  //   _last_log_offset.log_offset_ = log_offset;
  // }
  void set_last_log_offest(uint64_t log_offset) {
    u_offset = log_offset;
  }

  OP_t get_op() { return static_cast<OP_t>(op); }
  void set_op_persist(OP_t o) {
    op = static_cast<uint16_t>(o);
    pmem_persist(reinterpret_cast<char *>(this), 8);
  }

  size_t size() {
    auto total_length = sizeof(OP_VERSION) + sizeof(KEY) +
                         sizeof(uint16_t) + sizeof(uint64_t) + _vlen;
    return total_length;
  }
  uint64_t next_old_log() { return u_offset; }
};

#pragma pack()

/**
 * kv_log structure:
 * | op | version | key | value| last_log_offset| or
 * | op | version | key | vlen | value| last_log_offset| 
*/

#define NALI_VERSION_SHARDS 251

#define MAX_LOF_FILE_ID 64
// need a table to store all old logfile's start offset
//  need query the table to get real offset
struct global_ppage_meta {
  char *start_addr_arr_[numa_node_num][NALI_VERSION_SHARDS][MAX_LOF_FILE_ID]; // numa_id + hash_id + page_id -> mmap_addr
  void *operator new(size_t size) {
    return aligned_alloc(64, size);
  }
  global_ppage_meta() { memset(start_addr_arr_, 0, sizeof(start_addr_arr_)); }
};
global_ppage_meta *g_ppage_s_addr_arr_;

struct ppage_header {
  size_t numa_id; // logfile's numa id
  size_t hash_id_; // logfile's hash bucket number
  size_t ppage_id; // logfile's pageid
  size_t sz_allocated; // only for alloc
  size_t sz_freed; // only for gc, never modified by foreground thread
  SpinLatch rwlock; // protect foreground write and background gc
  ppage_header() : ppage_id(-1), sz_allocated(0), sz_freed(0) {}
};

// in dram, metadata for ppage
class __attribute__((aligned(64))) PPage {
  public:
    PPage(size_t numa_id, size_t hash_id, bool recovery = false) : start_addr_(NULL) {
      header_.numa_id = numa_id;
      header_.hash_id_ = hash_id;
      if (!recovery) {
        header_.ppage_id++;
        start_addr_ = alloc_new_ppage(numa_id, hash_id, header_.ppage_id);
      }
    } 

    void *operator new(size_t size) {
      return aligned_alloc(64, size);
    }

    // if space is not enough, need create a new ppage
    char *alloc(last_log_offest &log_info, size_t alloc_size) {
      header_.rwlock.WLock();
      if (alloc_size > PPAGE_SIZE - header_.sz_allocated) {
        header_.ppage_id++;
        start_addr_ = alloc_new_ppage(header_.numa_id, header_.hash_id_, header_.ppage_id);
        header_.sz_allocated = 0;
        header_.sz_freed = 0;
      }

      log_info.numa_id_ = header_.numa_id;
      log_info.ppage_id_ = header_.ppage_id;
      log_info.log_offset_ = header_.sz_allocated;

      char *ret = start_addr_ + header_.sz_allocated;
      header_.sz_allocated += alloc_size;

      header_.rwlock.WUnlock();
      return ret;
    }

    char *alloc_new_ppage(size_t numa_id, int32_t hash_id, int32_t ppage_id) {
      // alloc numa local page, file_name = nali_$(numa_id)_$(hash_id)_$(ppage_id)
      std::string file_name = "/mnt/pmem" + std::to_string(numa_id) + "/zzy/nali_";
      file_name += std::to_string(numa_id) + "_" + std::to_string(hash_id) + "_" + std::to_string(ppage_id);
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

      // init ppage, all bits are 1
      pmem_memset_persist(ptr, 0xffffffff, PPAGE_SIZE);

      g_ppage_s_addr_arr_->start_addr_arr_[numa_id][hash_id][ppage_id] = reinterpret_cast<char*>(ptr);

      return reinterpret_cast<char*>(ptr);
    }

  public:
    char *start_addr_; // ppage start addr
    ppage_header header_;
};

// cacheline align
struct __attribute__((__aligned__(64))) version_alloctor {
  std::atomic<size_t> version_;
  char padding_[56];
  version_alloctor(): version_(0) {}
  size_t get_version() {
    return version_++;
  }
};

// WAL, only once nvm write
template <class T, class P>
class LogKV {
public:
    LogKV(bool recovery) {
      init_numa_map();
      g_ppage_s_addr_arr_ = new global_ppage_meta();
      // for each numa node, create log files
      for (int numa_id = 0; numa_id < numa_node_num; numa_id++) {
        for (int hash_id = 0; hash_id < NALI_VERSION_SHARDS; hash_id++) {
          PPage *ppage = new PPage(numa_id, hash_id, recovery);
          ppage_[numa_id][hash_id] = ppage;
          g_ppage_s_addr_arr_->start_addr_arr_[numa_id][hash_id][0] = ppage->start_addr_;
        }
      }
    }

    char *alloc_log(uint64_t hash_key, last_log_offest &log_info, size_t alloc_size) {
      return ppage_[get_numa_id(global_thread_id)][hash_key % NALI_VERSION_SHARDS]->alloc(log_info, alloc_size);
    }

    char *get_addr(const last_log_offest &log_info, uint64_t hash_key) {
      return g_ppage_s_addr_arr_->start_addr_arr_[log_info.numa_id_][hash_key % NALI_VERSION_SHARDS][log_info.ppage_id_] + log_info.log_offset_;
    }

    char *get_page_start_addr(int numa_id, int hash_id, int logpage_id) {
      return g_ppage_s_addr_arr_->start_addr_arr_[numa_id][hash_id][logpage_id];
    }

    char *get_page_start_addr(int hash_id, const last_log_offest &log_info) {
      return g_ppage_s_addr_arr_->start_addr_arr_[log_info.numa_id_][hash_id][log_info.ppage_id_];
    }

    void set_page_start_addr(int numa_id, int hash_id, int logpage_id, char *start_addr) {
      g_ppage_s_addr_arr_->start_addr_arr_[numa_id][hash_id][logpage_id] = start_addr;
      // update ppage id
      int page_id = ppage_[numa_id][hash_id]->header_.ppage_id;
      if (page_id < logpage_id) {
        ppage_[numa_id][hash_id]->header_.ppage_id = logpage_id;
      }
    }

    void recovery_logkv(Tree<T, uint64_t> *db_, size_t recovery_thread_num) {
      // read all pmem log file, each thread scan a range hash log from newest to oldest
      // mmap all log file and set g_ppage_s_addr_arr_
      // new PPage and set PPage newest page id 
      std::vector<int> page_ids(NALI_VERSION_SHARDS, -1);
      for (int i = 0; i < numa_node_num; i++) {
          std::string dirname = "/mnt/pmem" + std::to_string(i) + "/zzy";
          std::vector<std::string> files;
          loadfiles(dirname, files);
          for (const std::string &file : files) {
            size_t pos0 = file.find("nali_") + strlen("nali_");
            size_t pos1 = file.find("_", pos0) + strlen("_");
            pos0 = file.find("_", pos1);
            size_t hash_id = std::stoi(file.substr(pos1, pos0-pos1));
            pos0 += strlen("_");
            pos1 = file.find("_", pos0);
            size_t ppage_id = std::stoi(file.substr(pos0, pos1-pos0));
            size_t mapped_len;
            int is_pmem;
            void *ptr = nullptr;
            if((ptr = pmem_map_file(file.c_str(), PPAGE_SIZE, PMEM_FILE_CREATE, 0666,
                                    &mapped_len, &is_pmem)) == NULL) {
                std::cout << "mmap pmem file fail" << std::endl;
                exit(1);
            }
            set_page_start_addr(i, hash_id, ppage_id, (char*)ptr);
          }
      }
      // for each log:
        // 1. if log is TRASH, skip 
        // 2. else, backtrack it's older logs and set TRASH
            // for insert or update log, do insert(XXX:set struct last_log_offest)
            // for delete log, set TRASH
        // for using ppage, update its ppage_header
      std::vector<std::thread> recovery_threads;
      std::atomic_int thread_idx_count(0);
      int perthread_hash_nums = NALI_VERSION_SHARDS / recovery_thread_num;
      for (int thread_cnt = 0; thread_cnt < recovery_thread_num; thread_cnt++) {
        int idx = thread_idx_count.fetch_add(1);
        global_thread_id = idx;
        recovery_threads.emplace_back([&](){
          int begin_hashid = global_thread_id * perthread_hash_nums;
          int end_hashid = (global_thread_id == recovery_thread_num-1) ? NALI_VERSION_SHARDS : (begin_hashid + perthread_hash_nums);
          for (int numa_id = 0; numa_id < numa_node_num; numa_id++) {
            for (int hash_id = begin_hashid; hash_id < end_hashid; hash_id++) {
              for (int ppage_id = ppage_[numa_id][hash_id]->header_.ppage_id; ppage_id >= 0; ppage_id--) {
                size_t log_offset = 0;
                char *addr = g_ppage_s_addr_arr_->start_addr_arr_[numa_id][hash_id][ppage_id];
                while (addr && log_offset < PPAGE_SIZE) {
                  nali::Pair_t<T, P> *log_ = (Pair_t<T, P> *)((addr+log_offset));
                  // log_->load((char*)(log_), false);
                  if (log_->op != OP_t::TRASH) {
                    std::vector<Pair_t<T, P> *> old_log;
                    if (log_->op == OP_t::DELETE) {
                      old_log.push_back(log_);
                      uint64_t old_log_offset;
                      bool ret = db_->erase(log_->key(), &old_log_offset);
                    } else {
                      //insert into dram index
                      bool ret = db_->insert(log_->key(), log_->u_offset);
                    }
                    uint64_t next_old_log = log_->next_old_log();
                    nali::Pair_t<T, P> *tmplog_ = nullptr;
                    while (next_old_log != UINT64_MAX) { // 最old的日志，next log地址为全1
                      last_log_offest log_info(next_old_log);
                      char *log_start_addr = get_page_start_addr(hash_id, log_info);
                      if (nullptr == log_start_addr) { // log已经被回收
                        break;
                      } else {
                        tmplog_ = (nali::Pair_t<T, P> *)(log_start_addr + log_info.log_offset_);
                        if (tmplog_->get_op() == OP_t::TRASH) // log已经被回收
                          break;
                        old_log.push_back(tmplog_);
                        next_old_log = tmplog_->next_old_log();
                      }
                    }
                    for (int p = old_log.size(); p >= 0; p--) {
                      tmplog_ = old_log[p];
                      tmplog_->set_op_persist(OP_t::TRASH);
                    }
                  }
                  
                  #ifdef VARVALUE
                    if (log_->vlen() == UINT16_MAX) {
                      // log is end
                      break;
                    }
                    log_offset += log_->size();
                  #else
                    log_offset += 32;
                  #endif
                }
                if (ppage_id == ppage_[numa_id][hash_id]->header_.ppage_id) {
                  // update ppage sz_allocated
                  ppage_[numa_id][hash_id]->header_.sz_allocated = log_offset;
                }
              }
            }
          }
        });
      }
    }

    // 返回log相对地址
    uint64_t insert(const T& key, const P& payload, uint64_t hash_key) {
      Pair_t<T, P> p(key, payload);
      p.set_version(version_allocator_[hash_key % NALI_VERSION_SHARDS].get_version());
      p.set_op(INSERT);
      last_log_offest log_info;
      log_info.vlen_ = p.vlen();
      char *log_addr = alloc_log(hash_key, log_info, p.size());
      p.store_persist(log_addr);
      return log_info.to_uint64_t_offset();
    }

    void erase(const T& key, uint64_t hash_key, uint64_t old_log_offset) {
      Pair_t<T, P> p(key);
      p.set_version(version_allocator_[hash_key % NALI_VERSION_SHARDS].get_version());
      p.set_op(DELETE);
      p.set_last_log_offest(old_log_offset);
      last_log_offest log_info;
      char *log_addr = alloc_log(hash_key, log_info, p.size());
      p.store_persist(log_addr);
      return;
    }

    char* update_step1(const T& key, const P& payload, uint64_t hash_key, Pair_t<T, P> &p, uint64_t &cur_log_addr) {
      p.set_version(version_allocator_[hash_key % NALI_VERSION_SHARDS].get_version());
      p.set_op(UPDATE);
      last_log_offest log_info;
      log_info.vlen_ = p.vlen();
      char *log_addr = alloc_log(hash_key, log_info, p.size());
      cur_log_addr = log_info.to_uint64_t_offset();
      return log_addr;
    }

  private:
    // 读出DirPath目录下的所有文件
    void loadfiles(std::string DirPath, std::vector<std::string> &files) {
        DIR *pDir;
        struct dirent* ptr;
        if(!(pDir = opendir(DirPath.c_str())))
        {
            std::cout<< "Folder " << DirPath << " doesn't Exist!" << std::endl;
            return;
        }

        while((ptr = readdir(pDir)) != 0) 
        {
            if (strcmp(ptr->d_name, ".") != 0 && strcmp(ptr->d_name, "..") != 0)
            {
                files.push_back(DirPath + "/" + ptr->d_name);
            }
        }
        // sort(files.begin(),files.end(),[&](const string &a, const string &b){
        //     return a > b;
        // });
        closedir(pDir);
    }

  private:
    PPage *ppage_[numa_node_num][NALI_VERSION_SHARDS] = {0};
    version_alloctor version_allocator_[NALI_VERSION_SHARDS];
};

}  // namespace nali