#ifndef AFINA_STORAGE_STRIPED_LRU_H
#define AFINA_STORAGE_STRIPED_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "ThreadSafeSimpleLRU.h"

namespace Afina {
namespace Backend {

class StripedLockLRU : public Afina::Storage {
public:
    explicit StripedLockLRU(size_t max_size = 1024) : _max_size(max_size) {
        size_t shard_size = _max_size / _num_shards;
        for (size_t i = 0; i < _num_shards; ++i) {
            _shards.emplace_back(new ThreadSafeSimplLRU(shard_size));
        }
    }

    ~StripedLockLRU() override = default;

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override {
        return _shards[std::hash<std::string>{}(key) % _num_shards]->Put(key, value);
    }

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override{
        return _shards[std::hash<std::string>{}(key) % _num_shards]->PutIfAbsent(key, value);
    }

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override{
        return _shards[std::hash<std::string>{}(key) % _num_shards]->Set(key, value);
    }

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override{
        return _shards[std::hash<std::string>{}(key) % _num_shards]->Delete(key);
    }

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override{
        return _shards[std::hash<std::string>{}(key) % _num_shards]->Get(key, value);
    }

private:
    std::size_t _max_size;

    static const size_t _num_shards = 4;
    std::vector<std::unique_ptr<ThreadSafeSimplLRU>> _shards;
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_STRIPED_LRU_H
