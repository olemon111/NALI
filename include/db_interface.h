#pragma once

#include "tree.h"
#include "varvalue_alloc.h"
#include "util/utils.h"
#include "../src/nali.h"
#include "../third/alexol/alex.h"
// #include "../third/utree/utree.h"
// #include "../third/pactree/src/pactree_wrapper.h"
// #include "../third/lbtree/lbtree-src/lbtree_wrapper.hpp"
// #include "../third/dptree/dptree_wrapper.hpp"
#include "../third/nap/include/nap_wrapper.h"
#include "../third/btreeolc/src/BTreeOLC_child_layout.h"
// #include "../third/fastfair/con_fastfair.h"
#include "tbb/tbb.h"
#include <utility>

namespace nali
{
    template <class T, class P>
    class nalidb : public Tree<T, P>
    {
    public:
        typedef std::pair<T, P> V;
        nalidb()
        {
            db_ = new nali::Nali();
            db_->Init();
        }

        ~nalidb()
        {
            delete db_;
        }

        void bulk_load(const V values[], int num_keys)
        {
            // db_->bulk_load(values, num_keys);
            for (int i = 0; i < num_keys; i++)
            {
                db_->Put(values[i].first, values[i].second);
            }
        }

        bool insert(const T &key, const P &payload)
        {
            return db_->Put(key, payload);
        }

        bool search(const T &key, P &payload)
        {
            return db_->Get(key, payload);
        }

        bool erase(const T &key, uint64_t *log_offset = nullptr)
        {
            return db_->Delete(key, log_offset);
        }

        bool update(const T &key, const P &payload, uint64_t *log_offset = nullptr)
        {
            return db_->Update(key, payload, log_offset);
        }

        int range_scan_by_size(const T &key, uint32_t to_scan, V *&result = nullptr)
        {
            int scan_size = to_scan;
            std::vector<std::pair<uint64_t, uint64_t>> res;
            db_->Scan(key, scan_size, res);
            assert(scan_size == res.size());
            for (int i = 0; i < res.size(); i++)
            {
                result[i] = res[i];
            }
            return res.size();
        }

        void get_info()
        {
            db_->Info();
        }

    private:
        nali::Nali *db_;
    };

    template <class T, class P>
    class alexoldb : public Tree<T, P>
    {
    public:
        typedef std::pair<T, P> V;
        alexoldb()
        {
            db_ = new alexol::Alex<T, P>();
            db_->set_max_model_node_size(1 << 24);
            db_->set_max_data_node_size(1 << 17);
        }

        ~alexoldb()
        {
            delete db_;
        }

        void bulk_load(const V values[], int num_keys)
        {
            db_->bulk_load(values, num_keys);
        }

        bool insert(const T &key, const P &payload)
        {
            return db_->insert(key, payload);
        }

        bool search(const T &key, P &payload)
        {
            auto ret = db_->get_payload(key, &payload);
            return ret;
        }

        bool erase(const T &key, uint64_t *log_offset = nullptr)
        {
            int num = db_->erase(key, log_offset);
            if (num > 0)
                return true;
            else
                return false;
        }

        bool update(const T &key, const P &payload, uint64_t *log_offset = nullptr)
        {
            return db_->update(key, payload, log_offset);
        }

        int range_scan_by_size(const T &key, uint32_t to_scan, V *&result = nullptr)
        {
            auto scan_size = db_->range_scan_by_size(key, static_cast<uint32_t>(to_scan), result);
            return to_scan;
        }

        void get_info() {}

    private:
        alexol::Alex<T, P, alexol::AlexCompare, std::allocator<std::pair<T, P>>, false> *db_;
    };

    template <class T, class P>
    class btreeolcdb : public Tree<T, P>
    {
    public:
        typedef std::pair<T, P> V;
        btreeolcdb()
        {
            db_ = new btreeolc::BTree<T, P>();
        }

        ~btreeolcdb()
        {
            delete db_;
        }

        void bulk_load(const V values[], int num_keys)
        {
            for (auto i = 0; i < num_keys; i++)
            {
                db_->insert(values[i].first, values[i].second);
            }
        }

        bool insert(const T &key, const P &payload)
        {
            db_->insert(key, payload);
            return true;
        }

        bool search(const T &key, P &payload)
        {
            return db_->lookup(key, payload);
        }

        bool erase(const T &key, uint64_t *log_offset = nullptr)
        {
            return false;
        }

        bool update(const T &key, const P &payload, uint64_t *log_offset = nullptr)
        {
            db_->insert(key, payload);
            return true;
        }

        int range_scan_by_size(const T &key, uint32_t to_scan, V *&result = nullptr)
        {
            return 0;
        }

        void get_info() {}

    private:
        btreeolc::BTree<T, P> *db_;
    };

    // template <class T, class P>
    // class utree_db : public Tree<T, P> {
    //     public:
    //         typedef std::pair<T, P> V;
    //         utree_db() {
    //             init_numa_map();
    //             db_ = new utree::btree();
    //             #ifdef VARVALUE
    //             vs_ = new varval_store();
    //             #endif
    //         }

    //         ~utree_db() {
    //             delete db_;
    //             #ifdef VARVALUE
    //             delete vs_;
    //             #endif
    //         }

    //         void bulk_load(const V values[], int num_keys) {
    //             for (int i = 0; i < num_keys; i++) {
    //                 #ifdef VARVALUE
    //                     char *addr = vs_->put(values[i].second);
    //                     db_->insert(values[i].first, addr);
    //                 #else
    //                     db_->insert(values[i].first, (char*)values[i].second);
    //                 #endif
    //             }
    //         }

    //         bool insert(const T& key, const P& payload) {
    //             #ifdef VARVALUE
    //                 char *addr = vs_->put(payload);
    //                 db_->insert(key, addr);
    //             #else
    //                 db_->insert(key, (char*)payload);
    //             #endif
    //             return true;
    //         }

    //         bool search(const T& key, P &payload) {
    //             auto value = db_->search(key);
    //             if (value != NULL) {
    //             #ifdef VARVALUE
    //                 vs_->get((char*)value, payload, VALUE_LENGTH);
    //             #else
    //                 memcpy(&payload, &value, 8);
    //             #endif
    //                 return true;
    //             }
    //             return false;
    //         }

    //         bool erase(const T& key, uint64_t *log_offset = nullptr) {
    //             db_->new_remove(key);
    //             return true;
    //         }

    //         bool update(const T& key, const P& payload, uint64_t *log_offset = nullptr) {
    //             #ifdef VARVALUE
    //                 char *addr = vs_->put(payload);
    //                 db_->update(key, addr);
    //             #else
    //                 db_->update(key, (char*)payload);
    //             #endif
    //             return true;
    //         }

    //         // TODO: 修改utree scan接口
    //         int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr) {
    //             constexpr size_t ONE_MB = 1ULL << 20;
    //             static thread_local char results[ONE_MB];
    //             char *res = results;
    //             int scanned = db_->scan(key, to_scan, results);
    //             for (int i = 0; i < scanned; i++) {
    //                 result[i].first = *(T*)res;
    //                 res += 8;
    //                 #ifdef VARVALUE
    //                     vs_->get((char*)(*(P*)res), result[i].second, VALUE_LENGTH);
    //                 #else
    //                     result[i].second = *(P*)res;
    //                 #endif
    //                 res += 8;
    //             }
    //             return scanned;
    //         }

    //         void get_info() {
    //         }

    //     private:
    //         utree::btree *db_;
    //         #ifdef VARVALUE
    //         valval_store *vs_;
    //         #endif
    // };

    // template <class T, class P>
    // class pactree_db : public Tree<T, P> {
    //     public:
    //         typedef std::pair<T, P> V;
    //         pactree_db(int numa_num) {
    //             init_numa_map();
    //             db_ = new pactree_wrapper(numa_num);
    //         }

    //         ~pactree_db() {
    //             delete db_;
    //         }

    //         void bulk_load(const V values[], int num_keys) {
    //             for (int i = 0; i < num_keys; i++) {
    //                 db_->insert(values[i].first, values[i].second);
    //             }
    //         }

    //         bool insert(const T& key, const P& payload) {
    //             return db_->insert(key, payload);
    //         }

    //         bool search(const T& key, P &payload) {
    //             return db_->find(key, payload);
    //         }

    //         bool erase(const T& key, uint64_t *log_offset = nullptr) {
    //             db_->remove(key);
    //             return true;
    //         }

    //         bool update(const T& key, const P& payload, uint64_t *log_offset = nullptr) {
    //             return db_->update(key, payload);
    //         }

    //         int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr) {
    //             return db_->scan(key, to_scan, result);
    //         }

    //         void get_info() {
    //         }

    //     private:
    //         pactree_wrapper *db_;
    // };

    // template <class T, class P>
    // class dptree_db : public Tree<T, P> {
    //     public:
    //         typedef std::pair<T, P> V;
    //         dptree_db(int total_thrad_num) {
    //             init_numa_map();
    //             db_ = new dptree_wrapper(total_thrad_num);
    //         }

    //         ~dptree_db() {
    //             delete db_;
    //         }

    //         void bulk_load(const V values[], int num_keys) {
    //             for (int i = 0; i < num_keys; i++) {
    //                 db_->insert(values[i].first, values[i].second);
    //             }
    //         }

    //         bool insert(const T& key, const P& payload) {
    //             return db_->insert(key, payload);
    //         }

    //         bool search(const T& key, P &payload) {
    //             return db_->find(key, payload);
    //         }

    //         bool erase(const T& key, uint64_t *log_offset = nullptr) {
    //             return db_->remove(key);
    //         }

    //         bool update(const T& key, const P& payload, uint64_t *log_offset = nullptr) {
    //             return db_->update(key, payload);
    //         }

    //         int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr) {
    //             return db_->scan(key, to_scan, result);
    //         }

    //         void get_info() {
    //         }

    //     private:
    //         dptree_wrapper *db_;
    // };

    template <class T, class P>
    class napfastfair_db : public Tree<T, P>
    {
    public:
        typedef std::pair<T, P> V;
        napfastfair_db()
        {
            init_numa_map();
            db_ = new nap_fastfair_wrapper();
        }

        ~napfastfair_db()
        {
            delete db_;
        }

        void bulk_load(const V values[], int num_keys)
        {
            for (int i = 0; i < num_keys; i++)
            {
                if ((i + 1) % 100000 == 0)
                {
                    std::cout << "Operate: " << i + 1 << std::endl;
                }
                db_->insert(values[i].first, values[i].second);
            }
        }

        bool insert(const T &key, const P &payload)
        {
            return db_->insert(key, payload);
        }

        bool search(const T &key, P &payload)
        {
            return db_->find(key, payload);
        }

        bool erase(const T &key, uint64_t *log_offset = nullptr)
        {
            return db_->remove(key);
        }

        bool update(const T &key, const P &payload, uint64_t *log_offset = nullptr)
        {
            return db_->update(key, payload);
        }

        int range_scan_by_size(const T &key, uint32_t to_scan, V *&result = nullptr)
        {
            return db_->scan(key, to_scan, result);
        }

        void get_info()
        {
            db_->print_numa_info(true);
        }

    private:
        nap_fastfair_wrapper *db_;
    };

    // template <class T, class P>
    // class fastfair_db : public Tree<T, P> {
    //     public:
    //         typedef std::pair<T, P> V;
    //         fastfair_db() {
    //             init_numa_map();
    //             db_ = new conff::btree();
    //         }

    //         ~fastfair_db() {
    //             delete db_;
    //         }

    //         void bulk_load(const V values[], int num_keys) {
    //             for (int i = 0; i < num_keys; i++) {
    //                 db_->btree_insert(values[i].first, (char*)values[i].second);
    //             }
    //         }

    //         bool insert(const T& key, const P& payload) {
    //             db_->btree_insert(key, (char*)payload);
    //             return true;
    //         }

    //         bool search(const T& key, P &payload) {
    //             auto value = db_->btree_search(key);
    //             if (value != NULL) {
    //             #ifdef VARVALUE
    //                 static_assert("not implement");
    //             #else
    //                 memcpy(&payload, &value, 8);
    //             #endif
    //                 return true;
    //             }
    //             return false;
    //         }

    //         bool erase(const T& key, uint64_t *log_offset = nullptr) {
    //             db_->btree_delete(key);
    //             return true;
    //         }

    //         bool update(const T& key, const P& payload, uint64_t *log_offset = nullptr) {
    //             db_->btree_insert(key, (char*)payload);
    //             return true;
    //         }

    //         int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr) {
    //             return db_->btree_search_range(key, to_scan, result);
    //         }

    //         void get_info() {
    //         }

    //     private:
    //         conff::btree *db_;
    // };
}
