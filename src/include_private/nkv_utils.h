/**
 *   BSD LICENSE
 *
 *   Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Samsung Electronics Co., Ltd. nor the names of
 *       its contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#ifndef NKV_UTILS_H
#define NKV_UTILS_H

#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <list>
#include <atomic>
#include <mutex>
#include <functional>

#include <memory>
#include <string>
#include <array>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/foreach.hpp>
#include "nkv_struct.h"
#include "nkv_result.h"
#include "csmglogger.h"

using namespace std;

extern c_smglogger* logger;
#define NKV_DEFAULT_STAT_FILE "./disk_stats.py"

int32_t nkv_cmd_exec(const char* cmd, std::string& result); 

nkv_result nkv_get_path_stat_util (const std::string& p_mount, nkv_path_stat* p_stat);

template<typename K, typename V>
  class nkv_lruCache {
  public:
    typedef pair<K,V> key_value_pair_t;
    typedef typename list<key_value_pair_t>::iterator list_iterator_t;
    nkv_lruCache(uint64_t size) : _max_size(size) {}

    void put (const K& key, const V& val) {
      std::lock_guard<std::mutex> lck (lru_lock);
      auto it = _cache_map.find(key);
      if ( it != _cache_map.end()) {
        _cache_list.erase(it->second);
        _cache_map.erase(it);
      }
      _cache_list.push_front(key_value_pair_t(key, val));
      _cache_map[key] = _cache_list.begin();
      if (_cache_map.size() > _max_size) {
        auto last = _cache_list.end();
        last--;
        _cache_map.erase(last->key);
        _cache_list.pop_back();
      }
    }

    const V& get(const K& key) {
      std::lock_guard<std::mutex> lck (lru_lock);
      auto it = _cache_map.find(key);
      if (it == _cache_map.end()) {
        throw range_error ("There is no such key");
      } else {
        _cache_list.splice(_cache_list.begin(), _cache_list, it->second);
        return it->second->second;
      }
    }

    bool exists(const K& key) {
      std::lock_guard<std::mutex> lck (lru_lock);
      return (_cache_map.find(key) == _cache_map.end());
    }

    uint64_t size() {
      std::lock_guard<std::mutex> lck (lru_lock);
      return _cache_map.size();
    }

  private:
    unordered_map<K, list_iterator_t> _cache_map;
    list<key_value_pair_t> _cache_list;
    uint64_t _max_size;
    std::mutex lru_lock;
  };

#endif
