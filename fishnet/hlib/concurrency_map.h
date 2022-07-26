#ifndef CONCURRENCY_MAP_H
#define CONCURRENCY_MAP_H

#include <map>
#include <mutex>
#include <shared_mutex>

namespace zfish {

template <typename K, typename V>
class ConcurrencyMap {
public:
    ConcurrencyMap() = default;

    void Put(const K &k, V &&v) {
        std::unique_lock write_lock(rw_);
        map_[k] = std::forward<V>(v);
    }

    V Get(const K &k) const {
        std::shared_lock<std::shared_mutex> read_lock(rw_);
        return map_[k];
    }

    bool Contain(const K &k) const {
        std::shared_lock<std::shared_mutex> read_lock(rw_);
        return map_.find(k) != map_.end();
    }

private:
    mutable std::shared_mutex rw_;
    std::map<K, V> map_;
};

}  // namespace zfish

#endif  // CONCURRENCY_MAP_H