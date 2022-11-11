#pragma once
#include <iostream>
#include <string.h>
#include <assert.h>
#include <libpmem.h>
#include <atomic>

#include "nali_alloc.h"
namespace nali {

extern thread_local size_t thread_id;
extern std::atomic<size_t> log_version; // FIXME(zzy): global version's overhead may too large?

#define ROUND_UP(s, n) (((s) + (n)-1) & (~(n - 1))) // ?
enum OP_t { TRASH, INSERT, DELETED, UPDATE }; // TRASH is for gc
const size_t INVALID = UINT64_MAX;
const size_t MAX_VALUE_LEN = 512;
const size_t MAX_KEY_LEN = 512;
using OP_VERSION = uint32_t;
constexpr int OP_BITS = 2;
constexpr int VERSION_BITS = sizeof(OP_VERSION) * 8 - OP_BITS;

#pragma pack(1) //使范围内结构体按1字节方式对齐
template <typename KEY, typename VALUE>
class Pair_t {
 public:
  OP_VERSION op : OP_BITS;
  OP_VERSION version : VERSION_BITS;
  KEY _key;
  VALUE _value;
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
  KEY *key() { return &_key; }
  KEY str_key() { return _key; }
  VALUE value() { return _value; }
  size_t klen() { return sizeof(KEY); }
  Pair_t(const KEY &k, const VALUE &v) {
    _key = k;
    _value = v;
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
  OP_t get_op() { return static_cast<OP_t>(op); }
  void set_op_persist(OP_t o) {
    op = static_cast<uint16_t>(o);
    pmem_persist(reinterpret_cast<char *>(this), 8);
  }
  // friend std::ostream &operator<<(std::ostream &out, Pair_t A);
  size_t size() {
    auto total_length = sizeof(OP_VERSION) + sizeof(KEY) + sizeof(VALUE);
    return total_length;
  }
};
template <typename KEY>
class Pair_t<KEY, std::string> {
 public:
  OP_VERSION op : OP_BITS;
  OP_VERSION version : VERSION_BITS;
  KEY _key;
  uint32_t _vlen;
  std::string svalue;
  Pair_t() {
    op = 0;
    version = 0;
    _vlen = 0;
    svalue.reserve(MAX_VALUE_LEN);
  };
  Pair_t(char *p) {
    auto pt = reinterpret_cast<Pair_t *>(p);
    _key = pt->_key;
    op = pt->op;
    version = pt->version;
    _vlen = pt->_vlen;
    svalue.assign(p + sizeof(KEY) + sizeof(uint32_t) + sizeof(OP_VERSION),
                  _vlen);
  }
  void load(char *p) {
    auto pt = reinterpret_cast<Pair_t *>(p);
    _key = pt->_key;
    op = pt->op;
    version = pt->version;
    _vlen = pt->_vlen;
    svalue.assign(p + sizeof(KEY) + sizeof(uint32_t) + sizeof(OP_VERSION),
                  _vlen);
  }
  size_t klen() { return sizeof(KEY); }
  KEY *key() { return &_key; }
  KEY str_key() { return _key; }
  std::string str_value() { return svalue; }

  Pair_t(KEY k, char *v_ptr, size_t vlen) : _vlen(vlen), version(0) {
    _key = k;
    svalue.assign(v_ptr, _vlen);
  }
  void set_key(KEY k) { _key = k; }
  void store_persist(char *addr) {
    auto p = reinterpret_cast<char *>(this);
    auto len = sizeof(KEY) + sizeof(uint32_t) + sizeof(OP_VERSION);
    memcpy(addr, p, len);
    memcpy(addr + len, &svalue[0], _vlen);
    pmem_persist(addr, len + _vlen);
  }
  void store_persist_update(char *addr) {
    auto p = reinterpret_cast<char *>(this);
    auto len = sizeof(KEY) + sizeof(uint32_t) + sizeof(OP_VERSION);
    memcpy(addr, p, len);
    memcpy(addr + len, &svalue[0], _vlen);
    reinterpret_cast<Pair_t<KEY, std::string> *>(addr)->set_version(version);
    pmem_persist(addr, len + _vlen);
  }
  void store(char *addr) {
    auto p = reinterpret_cast<char *>(this);
    auto len = sizeof(KEY) + sizeof(uint32_t) + sizeof(OP_VERSION);
    memcpy(addr, p, len);
    memcpy(addr + len, &svalue[0], _vlen);
  }
  void set_empty() { _vlen = 0; }
  void set_version(uint64_t old_version) { version = old_version + 1; }
  void set_op(OP_t o) { op = static_cast<uint16_t>(o); }
  OP_t get_op() { return op; }
  void set_op_persist(OP_t o) {
    op = static_cast<uint16_t>(o);
    pmem_persist(reinterpret_cast<char *>(this), 8);
  }
  size_t size() {
    auto total_length =
        sizeof(KEY) + sizeof(uint32_t) + sizeof(OP_VERSION) + _vlen;
    return total_length;
  }
};

template <>
class Pair_t<std::string, std::string> {
 public:
  OP_VERSION op : OP_BITS;
  OP_VERSION version : VERSION_BITS;
  uint32_t _klen;
  uint32_t _vlen;
  std::string skey;
  std::string svalue;
  Pair_t() {
    op = 0;
    version = 0;
    _klen = 0;
    _vlen = 0;
    skey.reserve(MAX_KEY_LEN);
    svalue.reserve(MAX_VALUE_LEN);
  };
  Pair_t(char *p) {
    auto pt = reinterpret_cast<Pair_t *>(p);
    op = pt->op;
    version = pt->version;
    _klen = pt->_klen;
    _vlen = pt->_vlen;
    skey.assign(p + 6, _klen);
    svalue.assign(p + 6 + _klen, _vlen);
  }
  void load(char *p) {
    auto pt = reinterpret_cast<Pair_t *>(p);
    op = pt->op;
    version = pt->version;
    _klen = pt->_klen;
    _vlen = pt->_vlen;
    skey.assign(p + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(OP_VERSION),
                _klen);
    svalue.assign(
        p + sizeof(uint32_t) + sizeof(uint32_t) + sizeof(OP_VERSION) + _klen,
        _vlen);
  }
  size_t klen() { return _klen; }
  char *key() { return &skey[0]; }
  std::string str_key() { return skey; }
  std::string value() { return svalue; }

  Pair_t(char *k_ptr, size_t klen, char *v_ptr, size_t vlen)
      : _klen(klen), _vlen(vlen), version(0) {
    skey.assign(k_ptr, _klen);
    svalue.assign(v_ptr, _vlen);
  }
  void set_key(char *k_ptr, size_t kl) {
    _klen = kl;
    skey.assign(k_ptr, _klen);
  }
  void store_persist(char *addr) {
    auto p = reinterpret_cast<void *>(this);
    auto len = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(OP_VERSION);
    memcpy(addr, p, len);
    memcpy(addr + len, &skey[0], _klen);
    memcpy(addr + len + _klen, &svalue[0], _vlen);
    pmem_persist(addr, len + _klen + _vlen);
  }
  void store_persist_update(char *addr) {
    auto p = reinterpret_cast<void *>(this);
    auto len = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(OP_VERSION);
    memcpy(addr, p, len);
    memcpy(addr + len, &skey[0], _klen);
    memcpy(addr + len + _klen, &svalue[0], _vlen);
    reinterpret_cast<Pair_t<std::string, std::string> *>(addr)->set_version(
        version);
    pmem_persist(addr, len + _klen + _vlen);
  }
  void store(char *addr) {
    auto p = reinterpret_cast<void *>(this);
    auto len = sizeof(uint32_t) + sizeof(uint32_t) + sizeof(OP_VERSION);
    memcpy(addr, p, len);
    memcpy(addr + len, &skey[0], _klen);
    memcpy(addr + len + _klen, &svalue[0], _vlen);
  }
  void set_empty() {
    _klen = 0;
    _vlen = 0;
  }
  void set_version(uint64_t old_version) { version = old_version + 1; }
  void set_op(OP_t o) { op = static_cast<uint16_t>(o); }
  OP_t get_op() { return static_cast<OP_t>(op); }
  void set_op_persist(OP_t o) {
    op = static_cast<uint16_t>(o);
    pmem_persist(reinterpret_cast<char *>(this), 8);
  }

  size_t size() {
    auto total_length = sizeof(uint32_t) + sizeof(uint32_t) +
                        sizeof(OP_VERSION) + _klen + _vlen;
    return total_length;
  }
};
#pragma pack()

/**
 * PPage structure:
 * |-------------------------64B----------------------------|--sizeof(ppage)-64B--|
 * | ppage_id | core_id | sz_freed | sz_allocated | padding |       kv_log        |
 * 
 * kv_log structure:
 * | op | version | key | value| or
 * | op | version | key | vlen | value| or
 * | op | version | klen | key | vlen | value|
*/

struct ppage_header {
  int32_t ppage_id;
  int32_t core_id;
  size_t sz_allocated; // only for alloc
  size_t sz_freed; // only for gc, never modified by foreground thread
  bool is_using;
  ppage_header() : ppage_id(-1), core_id(-1), sz_allocated(64), sz_freed(0) {}
};

class __attribute__((aligned(64))) PPage {
  public:
    PPage(size_t t_id) {
      start_addr_ = alloc_new_ppage(t_id, header_.ppage_id+1, false);
      header_.ppage_id = 0;
      header_.core_id = t_id;
      persist_metadata();
    }

    // if space is not enough, need create a new ppage
    char *alloc(size_t alloc_size) {
      if (alloc_size > PPAGE_SIZE - header_.sz_allocated) {
        start_addr_ = alloc_new_ppage(header_.core_id, header_.ppage_id+1, false);
        header_.ppage_id++;
        header_.sz_allocated = 64;
        header_.sz_freed = 0;
      }

      char *ret = start_addr_ + header_.sz_allocated;
      header_.sz_allocated += alloc_size;
      return ret;
    }

    void persist_metadata() {
      pmem_memcpy_persist(start_addr_, &header_, offsetof(ppage_header, sz_freed));
    }

  private:
    char *start_addr_; // ppage start addr
    ppage_header header_;
};

// WAL, twice nvm write：1. append log，2. update ppage_metadata
template <class T, class P>
class LogKV {
public:
    LogKV(size_t thread_num) {
      for (size_t i = 0; i < thread_num; i++) {
        ppage_[i] = new PPage(i);
      }

      init_numa_map();
    }

    char *insert(const T& key, const P& payload) {
      Pair_t<T, P> p(key, payload);
      p.set_op(INSERT);
      p.set_version(log_version++);
      char *log_addr = ppage_[thread_id]->alloc(p.size());
      p.store_persist(log_addr);
      ppage_[thread_id]->persist_metadata(); // update metadata
      return log_addr;
    }

    void erase(const T& key) {
      Pair_t<T, P> p;
      p.set_key(key);
      p.set_op(DELETED);
      p.set_version(log_version++);
      char *log_addr = ppage_[thread_id]->alloc(p.size());
      p.store_persist(log_addr);
      ppage_[thread_id]->persist_metadata();
    }

    char *update(const T& key, const P& payload) {
      Pair_t<T, P> p(key, payload);
      p.set_op(UPDATE);
      p.set_version(log_version++);
      char *log_addr = ppage_[thread_id]->alloc(p.size());
      p.store_persist(log_addr);
      ppage_[thread_id]->persist_metadata();
      return log_addr;
    }

  private:
    PPage *ppage_[max_thread_num];
};

}  // namespace nali