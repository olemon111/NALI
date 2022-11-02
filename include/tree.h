#pragma once

#include <map>
#include <cstdint>

template<class T, class P>
class Tree;

//used to define the interface of all benchmarking trees
// the param `epoch` is uesd in apex
template <class T, class P>
class Tree {
 public:
  typedef std::pair<T, P> V;
  virtual void bulk_load(const V[], int) = 0;
  virtual bool insert(const T&, const P&, bool epoch = false) = 0;
  virtual bool search(const T&, P*, bool epoch = false) const = 0;
  virtual bool erase(const T&, bool epoch = false) = 0;
  virtual bool update(const T&, const P&, bool epoch = false) = 0;
  // Return #keys really scanned
  virtual int range_scan_by_size(const T&, uint32_t, V*& result, bool epoch = false) = 0;

  virtual void get_depth_info() = 0;
};