#pragma once

#include "tree.h"
#include "../src/nali.h"
#include "../third/ALEX/alex.h"
#include "../third/FAST_FAIR/btree.h"

namespace nali {
    template <class T, class P>
    class nalidb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            nalidb() {
                db_ = new nali::Nali<T, P>();
            }

            ~nalidb() {
                delete db_;
            }

            void bulk_load(const V values[], int num_keys) {
                db_->bulk_load(values, num_keys);
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                return (db_->insert(key, payload)).second;
            }

            bool search(const T& key, P* payload, bool epoch = false) const {
                auto it = db_->find(key);
                if (it == db_->end())
                    return false;
                else {
                    *payload = it.payload();
                    return true;
                }
            }

            bool erase(const T& key, bool epoch = false){
                int num_erased = db_->erase(key);
                if (0 == num_erased)
                    return false; // not exist
                return true;
            }

            bool update(const T& key, const P& payload, bool epoch = false){
                auto it = db_->find(key);
                if (it == db_->end())
                    return false; // not exist
                it.payload() = payload;
                return true;
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false){
                auto it = db_->find(key);
                if (it == db_->end())
                    return 0;
                if (result == nullptr) {
                    result = new V[to_scan];
                }

                uint32_t i = 0;
                for (i = 0; i < to_scan && it != db_->end(); i++) {
                    result[i] = *it;
                    it++;
                }
                return i;
            }

            void get_depth_info() {}

        private:
            nali::Nali<T, P> *db_;
    };

    template <class T, class P>
    class alexdb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            alexdb() {
                db_ = new alex::Alex<T, P>();
            }

            ~alexdb() {
                delete db_;
            }

            void bulk_load(const V values[], int num_keys) {
                db_->bulk_load(values, num_keys);
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                return (db_->insert(key, payload)).second;
            }

            bool search(const T& key, P* payload, bool epoch = false) const {
                auto it = db_->find(key);
                if (it == db_->end())
                    return false;
                else {
                    *payload = it.payload();
                    return true;
                }
            }

            bool erase(const T& key, bool epoch = false){
                int num_erased = db_->erase(key);
                if (0 == num_erased)
                    return false; // not exist
                return true;
            }

            bool update(const T& key, const P& payload, bool epoch = false){
                auto it = db_->find(key);
                if (it == db_->end())
                    return false; // not exist
                it.payload() = payload;
                return true;
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false){
                auto it = db_->find(key);
                if (it == db_->end())
                    return 0;
                if (result == nullptr) {
                    result = new V[to_scan];
                }

                uint32_t i = 0;
                for (i = 0; i < to_scan && it != db_->end(); i++) {
                    result[i] = *it;
                    it++;
                }
                return i;
            }

            void get_depth_info() {}

        private:
            alex::Alex<T, P> *db_;
    };

    template <class T, class P>
    class fastfairdb : public Tree<T, P> {
        public:
            typedef std::pair<T, P> V;
            fastfairdb() {
                if (FileExists(pool_name)) {
                    ::remove(pool_name);
                }
                my_alloc::BasePMPool::Initialize(pool_name, pool_size);
                db_ = new fastfair::btree();
            }

            ~fastfairdb() {
                delete db_;
                my_alloc::BasePMPool::ClosePool();
                ::remove(pool_name);
            }

            void bulk_load(const V values[], int num_keys) {
                for (int i = 0; i < num_keys; i++) {
                    db_->btree_insert(values[i].first, (char *)values[i].second);
                }
            }

            bool insert(const T& key, const P& payload, bool epoch = false) {
                db_->btree_insert(key, (char *)payload);
                return true;
            }

            bool search(const T& key, P* payload, bool epoch = false) const {
                *payload = (P)db_->btree_search(key);
                return true;
            }

            bool erase(const T& key, bool epoch = false){
                db_->btree_delete(key);
                return true;
            }

            bool update(const T& key, const P& payload, bool epoch = false){
                db_->btree_delete(key);
                db_->btree_insert(key, (char *)payload);
                return true;
            }

            int range_scan_by_size(const T& key, uint32_t to_scan, V* &result = nullptr, bool epoch = false){
                std::vector<std::pair<fastfair::entry_key_t, uint64_t>> res;
                int scan = to_scan;
                db_->btree_search_range(key, UINT64_MAX, res, scan);
                if (result == nullptr) {
                    result = new V[scan];
                }
                for (int i = 0; i < scan; i++) {
                    result[i] = res[i];
                }
                return scan;
            }

            void get_depth_info() {}

        private:
            fastfair::btree *db_;
    };
}

