#ifndef _RTE_RING_H_
#define _RTE_RING_H_

#include <cassert>
#include <cstdint>

#include "rwlock.h"

template <typename T>
class RteRing {
 public:
  static constexpr unsigned RTX_RTE_RING_REP_COUNT = 3;

  enum RTE_RING_OP_STATUS {
    RTE_RING_OK = 0,
    RTX_RING_FULL,
    RTX_RING_NOT_ENOUGH,
  };

  /**
   * @brief Construct a new Rte Ring object
   *
   * @param size RTE Ring Size, must be 2^x
   * @param sp Single Producer, default = false
   * @param sc Single Customer, default = false
   */
  RteRing(uint32_t size, bool sp = false, bool sc = false) {
    // verify size is 2^x
    assert((size & (-size)) == size);
    prod.sp_ = sp;
    prod.size_ = size;
    prod.mask_ = prod.size_ - 1;
    prod.prod_tail_ = prod.prod_head_ = 0;

    cons.sc_ = sc;
    cons.size_ = size;
    cons.mask_ = cons.size_ - 1;
    cons.cons_tail_ = cons.cons_head_ = 0;

    objs_ = new T *[size];
  }

  ~RteRing() { delete[] objs_; }

  RTE_RING_OP_STATUS enqueue(T *const obj_table) {
    if (prod.sp_) {
      return enqueue_sp(1, &obj_table);
    } else
      return enqueue_mp(1, &obj_table);
  }

  RTE_RING_OP_STATUS dequeue(T **obj_table) {
    if (cons.sc_)
      return dequeue_sc(1, obj_table);
    else
      return dequeue_mc(1, obj_table);
  }

  RTE_RING_OP_STATUS enqueue(uint32_t n, T *const *obj_table, uint32_t *res_n = nullptr, bool fixed = false) {
    if (prod.sp_)
      return enqueue_sp(n, obj_table, res_n, fixed);
    else
      return enqueue_mp(n, obj_table, res_n, fixed);
  }

  RTE_RING_OP_STATUS dequeue(uint32_t n, T **obj_table, uint32_t *res_n = nullptr, bool fixed = false) {
    if (cons.sc_)
      return dequeue_sc(n, obj_table, res_n, fixed);
    else
      return dequeue_mc(n, obj_table, res_n, fixed);
  }

 private:
  RTE_RING_OP_STATUS enqueue_sp(uint32_t n, T *const *obj_table, uint32_t *res_n = nullptr, bool fixed = false) {
    uint32_t prod_head, cons_tail;
    uint32_t prod_next, free_entries;
    uint32_t mask = prod.mask_;

    prod_head = prod.prod_head_;
    cons_tail = cons.cons_tail_;
    // 即使 prod_head 与 cons_tail 溢出 u32 也不会出错
    free_entries = mask + cons_tail - prod_head;

    if (n > free_entries) {
      if (fixed || free_entries == 0) return RTX_RING_FULL;
      n = free_entries;
    }

    prod_next = prod_head + n;
    prod.prod_head_ = prod_next;

    for (uint32_t i = 0; i < n; ++i) {
      objs_[(prod_head + i) & mask] = obj_table[i];
    }
    mb();

    prod.prod_tail_ = prod_next;
    if (res_n) *res_n = n;
    return RTE_RING_OK;
  }

  RTE_RING_OP_STATUS enqueue_mp(uint32_t n, T *const *obj_table, uint32_t *res_n = nullptr, bool fixed = false) {
    uint32_t prod_head, cons_tail;
    uint32_t prod_next, free_entries;
    uint32_t mask = prod.mask_;
    uint32_t max_n = n;
    unsigned rep = 0;

    do {
      n = max_n;
      prod_head = prod.prod_head_;
      cons_tail = cons.cons_tail_;
      free_entries = mask + cons_tail - prod_head;

      if (n > free_entries) {
        if (fixed || free_entries == 0) return RTX_RING_FULL;
        n = free_entries;
      }

      prod_next = prod_head + n;
      // printf("prod.prod_head_: %u prod_head: %u, prod_next: %u\n",
      //         prod.prod_head_, prod_head, prod_next);
      if (prod.prod_head_.compare_exchange_weak(prod_head, prod_next, std::memory_order_acq_rel)) {
        break;
      }
    } while (true);

    for (uint32_t i = 0; i < n; ++i) {
      objs_[(prod_head + i) & mask] = obj_table[i];
    }

    mb();

    while (prod.prod_tail_ != prod_head) {
      cpu_relax();

      if (++rep == RTX_RTE_RING_REP_COUNT) {
        rep = 0;
        sched_yield();
      }
    }
    prod.prod_tail_ = prod_next;
    if (res_n) *res_n = n;
    return RTE_RING_OK;
  }

  RTE_RING_OP_STATUS dequeue_sc(uint32_t n, T **obj_table, uint32_t *res_n = nullptr, bool fixed = false) {
    uint32_t cons_head, prod_tail;
    uint32_t cons_next, entries;
    uint32_t mask = cons.mask_;

    cons_head = cons.cons_head_;
    prod_tail = prod.prod_tail_;
    entries = prod_tail - cons_head;

    res_n = 0;
    if (n > entries) {
      if (fixed || entries == 0) return RTX_RING_NOT_ENOUGH;
      n = entries;
    }

    cons_next = cons_head + n;
    cons.cons_head_ = cons_next;

    for (uint32_t i = 0; i < n; ++i) {
      obj_table[i] = objs_[(cons_head + i) % mask];
    }
    mb();

    cons.cons_tail_ = cons_next;
    if (res_n) *res_n = n;
    return RTE_RING_OK;
  }

  RTE_RING_OP_STATUS dequeue_mc(uint32_t n, T **obj_table, uint32_t *res_n = nullptr, bool fixed = false) {
    uint32_t cons_head, prod_tail;
    uint32_t cons_next, entries;
    uint32_t mask = cons.mask_;
    uint32_t max_n = n;
    unsigned rep = 0;

    res_n = 0;
    do {
      n = max_n;

      cons_head = cons.cons_head_;
      prod_tail = prod.prod_tail_;
      entries = prod_tail - cons_head;

      if (n > entries) {
        if (fixed || entries == 0) return RTX_RING_NOT_ENOUGH;
        n = entries;
      }

      cons_next = cons_head + n;
      // printf("cons.cons_head_: %u cons_head: %u, cons_next: %u\n",
      //         cons.cons_head_, cons_head, cons_next);
      if (cons.cons_head_.compare_exchange_weak(cons_head, cons_next, std::memory_order_acq_rel)) {
        break;
      }
    } while (true);

    for (uint32_t i = 0; i < n; ++i) {
      obj_table[i] = objs_[(cons_head + i) % mask];
    }

    mb();

    while (cons.cons_tail_ != cons_head) {
      cpu_relax();

      if (++rep == RTX_RTE_RING_REP_COUNT) {
        rep = 0;
        sched_yield();
      }
    }
    cons.cons_tail_ = cons_next;
    if (res_n) *res_n = n;
    return RTE_RING_OK;
  }

  struct Prod {
    uint32_t sp_;
    uint32_t size_;
    uint32_t mask_;
    std::atomic_uint32_t prod_head_;
    std::atomic_uint32_t prod_tail_;
  } prod __attribute__((aligned(64)));

  struct Cons {
    uint32_t sc_;
    uint32_t size_;
    uint32_t mask_;
    std::atomic_uint32_t cons_head_;
    std::atomic_uint32_t cons_tail_;
  } cons __attribute__((aligned(64)));

  T **objs_;
};

#endif  // _RTE_RING_H_