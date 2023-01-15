#pragma once
#include <iostream>
#include <string.h>
#include <assert.h>
#include <libpmem.h>
#include <atomic>
#include <jemalloc/jemalloc.h>
#include "nali_alloc.h"
#include "xxhash.h"
#include "nali_cache.h"
#include "rwlock.h"
namespace nali {

// #define USE_PROXY

extern thread_local size_t thread_id;

enum OP_t { INSERT, DELETE, UPDATE, TRASH }; // TRASH is for gc
const size_t INVALID = UINT64_MAX;
const size_t MAX_VALUE_LEN = 512;
const size_t MAX_KEY_LEN = 512;
using OP_VERSION = uint64_t;
constexpr int OP_BITS = 2;
constexpr int VERSION_BITS = sizeof(OP_VERSION) * 8 - OP_BITS;

constexpr uint64_t numa_id_mask = (0xffffUL << 48);
constexpr uint64_t ppage_id_mask = (0xffffUL << 32);
constexpr uint64_t log_offset_mask = 0xffffffffUL;
struct last_log_offest {
  uint16_t numa_id_; // last log's logfile numa id
  uint16_t ppage_id_; // last log's logfile number
  uint32_t log_offset_; // last log's in-logfile's offset
  last_log_offest() : numa_id_(UINT16_MAX) {}; // set to UINT16_MAX to indicate it is first log
  last_log_offest(uint16_t numa_id, uint16_t ppage_id, uint32_t log_offset) : 
      numa_id_(numa_id), ppage_id_(ppage_id), log_offset_(log_offset) {}
  last_log_offest(uint64_t addr_info) : numa_id_((addr_info & numa_id_mask)>>48), 
      ppage_id_((addr_info & ppage_id_mask)>>32), log_offset_(addr_info & log_offset_mask) {}
  uint64_t to_uint64_t_offset() {
    return (((uint64_t)numa_id_) << 48) | (((uint64_t)ppage_id_) << 32) | ((uint64_t)log_offset_);
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

  Pair_t() {
    _key = 0;
    _value = 0;
  };
  Pair_t(char *p) {
    auto pt = reinterpret_cast<Pair_t<KEY, VALUE> *>(p);
    *this = *pt;
  }
  void load(char *p) {
    auto pt = reinterpret_cast<Pair_t<KEY, VALUE> *>(p);
    *this = *pt;
  }
  KEY key() { return _key; }
  VALUE value() { return _value; }
  size_t klen() { return sizeof(KEY); }
  Pair_t(const KEY &k, const VALUE &v) {
    _key = k;
    _value = v;
  }
  Pair_t(const KEY &k) {
    _key = k;
  }
  void set_key(KEY k) { _key = k; }

  void store_persist(void *addr) {
    auto p = reinterpret_cast<void *>(this);
    auto len = size();
    memcpy(addr, p, len);
    pmem_persist(addr, len);
  }
  void store_persist_update(char *addr) { store_persist(addr); }
  void store(void *addr) {
    auto p = reinterpret_cast<void *>(this);
    auto len = size();
    memcpy(addr, p, len);
  }
  void set_empty() {
    _key = 0;
    _value = 0;
  }
  void set_version(uint64_t old_version) { version = old_version + 1; }
  void set_op(OP_t o) { op = static_cast<uint16_t>(o); }
  void set_last_log_offest(uint16_t numa_id, uint16_t ppage_id, uint32_t log_offset) { 
    _last_log_offset.numa_id_ = numa_id;
    _last_log_offset.ppage_id_ = ppage_id;
    _last_log_offset.log_offset_ = log_offset;
  }
  void set_last_log_offest(uint64_t log_offset) {
    u_offset = log_offset;
  }

  OP_t get_op() { return static_cast<OP_t>(op); }
  void set_op_persist(OP_t o) {
    op = static_cast<uint16_t>(o);
    pmem_persist(reinterpret_cast<char *>(this), 8);
  }
  // friend std::ostream &operator<<(std::ostream &out, Pair_t A);
  size_t size() {
    auto total_length = sizeof(OP_VERSION) + sizeof(KEY) + sizeof(VALUE) + sizeof(last_log_offest);
    return total_length;
  }
  uint64_t next_old_log() { return u_offset; }
};

#pragma pack()

/**
 * kv_log structure:
 * | op | version | key | value| last_log_offset| or
 * | op | version | key | vlen | value| last_log_offset| or
 * | op | version | klen | key | vlen | value| last_log_offset|
*/

#define NALI_VERSION_SHARDS 256

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
    PPage(size_t numa_id, size_t hash_id) {
      header_.numa_id = numa_id;
      header_.hash_id_ = hash_id;
      header_.ppage_id++;
      start_addr_ = alloc_new_ppage(numa_id, hash_id, header_.ppage_id);
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
    LogKV() {
      init_numa_map();
      g_ppage_s_addr_arr_ = new global_ppage_meta();
      // for each numa node, create log files
      for (int numa_id = 0; numa_id < numa_node_num; numa_id++) {
        for (int hash_id = 0; hash_id < NALI_VERSION_SHARDS; hash_id++) {
          PPage *ppage = new PPage(numa_id, hash_id);;
          ppage_[numa_id][hash_id] = ppage;
          g_ppage_s_addr_arr_->start_addr_arr_[numa_id][hash_id][0] = ppage->start_addr_;
        }
      }
    }

    char *alloc_log(uint64_t hash_key, last_log_offest &log_info, size_t alloc_size = 32) {
      return ppage_[get_numa_id(thread_id)][hash_key % NALI_VERSION_SHARDS]->alloc(log_info, alloc_size);
    }

    char *get_addr(const last_log_offest &log_info, uint64_t hash_key) {
      return g_ppage_s_addr_arr_->start_addr_arr_[log_info.numa_id_][hash_key % NALI_VERSION_SHARDS][log_info.ppage_id_] + log_info.log_offset_;
    }

    char *get_page_start_addr(int numa_id, int hash_id, int logpage_id) {
      return g_ppage_s_addr_arr_->start_addr_arr_[numa_id][hash_id][logpage_id];
    }

    // 返回log相对地址
    uint64_t insert(const T& key, const P& payload, uint64_t hash_key) {
      Pair_t<T, P> p(key, payload);
      p.set_version(version_allocator_[hash_key % NALI_VERSION_SHARDS].get_version());
      p.set_op(INSERT);
      last_log_offest log_info;
      char *log_addr = alloc_log(hash_key, log_info, 32); // TODO:var length log
      p.store_persist(log_addr);
      return log_info.to_uint64_t_offset();
    }

    void erase(const T& key, uint64_t hash_key, uint64_t old_log_offset) {
      Pair_t<T, P> p(key);
      p.set_version(version_allocator_[hash_key % NALI_VERSION_SHARDS].get_version());
      p.set_op(DELETE);
      p.set_last_log_offest(old_log_offset);
      last_log_offest log_info;
      char *log_addr = alloc_log(hash_key, log_info, 32); // TODO:var length log
      p.store_persist(log_addr);
      return;
    }

    char* update_step1(const T& key, const P& payload, uint64_t hash_key, Pair_t<T, P> &p, uint64_t &cur_log_addr) {
      p.set_version(version_allocator_[hash_key % NALI_VERSION_SHARDS].get_version());
      p.set_op(UPDATE);
      last_log_offest log_info;
      char *log_addr = alloc_log(hash_key, log_info, 32); // TODO:var length log
      cur_log_addr = log_info.to_uint64_t_offset();
      return log_addr;
    }

  private:
    PPage *ppage_[numa_node_num][NALI_VERSION_SHARDS] = {0};
    version_alloctor version_allocator_[NALI_VERSION_SHARDS];
};

}  // namespace nali