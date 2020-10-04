#ifndef AFINA_STORAGE_STRIPED_LRU_H
#define AFINA_STORAGE_STRIPED_LRU_H

#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <afina/Storage.h>

namespace Afina {
namespace Backend {

/**
 * # Map based implementation
 * That is NOT thread safe implementaiton!!
 */
class StripedLockLRU : public Afina::Storage {
public:
    StripedLockLRU(size_t max_size = 1024) : _max_size(max_size) {
        size_t shard_size = _max_size / _num_shards;
        for (size_t i =0; i < _num_shards; ++i) {
            _shards[i]._max_size = shard_size;
            _shards[i]._lru_head = new lru_node{"", "", nullptr, nullptr};
            _shards[i]._lru_head->prev = _shards[i]._lru_head;
            _shards[i]._lru_head->next.reset(_shards[i]._lru_head);
        }
    }

    ~StripedLockLRU() {
        for (size_t i =0; i < _num_shards; ++i) {
            _shards[i]._lru_index.clear();
            auto node = _shards[i]._lru_head->prev;
            while (node != _shards[i]._lru_head) {
                std::swap(node->prev, node->next->prev);
                std::swap(node->next, node->next->prev->next);
                node->next.reset();
                node = _shards[i]._lru_head->prev;
            }
            _shards[i]._lru_head->next.reset();
        }
    }

    // Implements Afina::Storage interface
    bool Put(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool PutIfAbsent(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Set(const std::string &key, const std::string &value) override;

    // Implements Afina::Storage interface
    bool Delete(const std::string &key) override;

    // Implements Afina::Storage interface
    bool Get(const std::string &key, std::string &value) override;

private:
    // LRU cache node
    using lru_node = struct lru_node {
        const std::string key;
        std::string value;
        lru_node *prev;
        std::unique_ptr<lru_node> next;
    };

    using lru_shard = struct lru_shard {
        // Maximum number of bytes could be stored in this cache.
        // i.e all (keys+values) must be less the _max_size
        std::size_t _max_size = 0;

        // Main storage of lru_nodes, elements in this list ordered descending by "freshness": in the head
        // element that wasn't used for longest time.
        //
        // List owns all nodes
        lru_node *_lru_head = nullptr;

        // Current size of cache
        size_t _cache_size = 0;

        // Index of nodes from list above, allows fast random access to elements by lru_node#key
        std::map<std::reference_wrapper<const std::string>, std::reference_wrapper<lru_node>, std::less<std::string>>
            _lru_index;

        // Multithreading
        std::mutex shard_mutex;
    };

    std::size_t _max_size;

    static const size_t _num_shards = 4;
    std::array<lru_shard, _num_shards> _shards;

    void _put_absent(const std::string &key, const std::string &value, size_t cur_shard);

    void _set_existing(lru_node &node, const std::string &value, size_t cur_shard);

    void _delete_least_recent(size_t cur_shard);
};

} // namespace Backend
} // namespace Afina

#endif // AFINA_STORAGE_STRIPED_LRU_H
