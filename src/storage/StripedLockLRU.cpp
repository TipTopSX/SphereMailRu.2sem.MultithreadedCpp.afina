#include "StripedLockLRU.h"

namespace Afina {
namespace Backend {

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Put(const std::string &key, const std::string &value) {
    size_t cur_shard = std::hash<std::string>{}(key) % _num_shards;
    if (key.size() + value.size() > _shards[cur_shard]._max_size) {
        return false;
    }
    std::unique_lock<std::mutex> ul(_shards[cur_shard].shard_mutex);
    auto in_cache = _shards[cur_shard]._lru_index.find(key);
    if (in_cache == _shards[cur_shard]._lru_index.end()) {
        _put_absent(key, value, cur_shard);
        return true;
    } else {
        lru_node &node = in_cache->second.get();
        _set_existing(node, value, cur_shard);
        return true;
    }
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::PutIfAbsent(const std::string &key, const std::string &value) {
    size_t cur_shard = std::hash<std::string>{}(key) % _num_shards;
    if (key.size() + value.size() > _shards[cur_shard]._max_size) {
        return false;
    }
    std::unique_lock<std::mutex> ul(_shards[cur_shard].shard_mutex);
    auto in_cache = _shards[cur_shard]._lru_index.find(key);
    if (in_cache == _shards[cur_shard]._lru_index.end()) {
        _put_absent(key, value, cur_shard);
        return true;
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Set(const std::string &key, const std::string &value) {
    size_t cur_shard = std::hash<std::string>{}(key) % _num_shards;
    if (key.size() + value.size() > _shards[cur_shard]._max_size) {
        return false;
    }
    std::unique_lock<std::mutex> ul(_shards[cur_shard].shard_mutex);
    auto in_cache = _shards[cur_shard]._lru_index.find(key);
    if (in_cache != _shards[cur_shard]._lru_index.end()) {
        lru_node &node = in_cache->second.get();
        _set_existing(node, value, cur_shard);
        return true;
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Delete(const std::string &key) {
    size_t cur_shard = std::hash<std::string>{}(key) % _num_shards;
    std::unique_lock<std::mutex> ul(_shards[cur_shard].shard_mutex);
    auto in_cache = _shards[cur_shard]._lru_index.find(key);
    if (in_cache != _shards[cur_shard]._lru_index.end()) {
        lru_node &node = in_cache->second.get();
        _shards[cur_shard]._cache_size -= node.key.size() + node.value.size();
        _shards[cur_shard]._lru_index.erase(key);
        std::swap(node.prev, node.next->prev);
        std::swap(node.next, node.next->prev->next);
        node.next.reset();
        return true;
    }
    return false;
}

// See MapBasedGlobalLockImpl.h
bool StripedLockLRU::Get(const std::string &key, std::string &value) {
    size_t cur_shard = std::hash<std::string>{}(key) % _num_shards;
    std::unique_lock<std::mutex> ul(_shards[cur_shard].shard_mutex);
    auto in_cache = _shards[cur_shard]._lru_index.find(key);
    if (in_cache != _shards[cur_shard]._lru_index.end()) {
        value = in_cache->second.get().value;
        lru_node &node = in_cache->second.get();
        std::swap(node.prev, node.next->prev);
        std::swap(node.next, node.next->prev->next);
        std::swap(node.prev, _shards[cur_shard]._lru_head->next->prev);
        std::swap(node.next, _shards[cur_shard]._lru_head->next);
        return true;
    }
    return false;
}

void StripedLockLRU::_put_absent(const std::string &key, const std::string &value, size_t cur_shard) {
    while (_shards[cur_shard]._max_size - _shards[cur_shard]._cache_size < key.size() + value.size()) {
        _delete_least_recent(cur_shard);
    }
    _shards[cur_shard]._cache_size += key.size() + value.size();
    auto node = new lru_node{key, value, nullptr, nullptr};
    node->prev = node;
    node->next.reset(node);
    std::swap(node->prev, _shards[cur_shard]._lru_head->next->prev);
    std::swap(node->next, _shards[cur_shard]._lru_head->next);
    _shards[cur_shard]._lru_index.emplace(std::reference_wrapper<const std::string>(node->key), std::reference_wrapper<lru_node>(*node));
}
void StripedLockLRU::_set_existing(StripedLockLRU::lru_node &node, const std::string &value, size_t cur_shard) {
    std::swap(node.prev, node.next->prev);
    std::swap(node.next, node.next->prev->next);
    std::swap(node.prev, _shards[cur_shard]._lru_head->next->prev);
    std::swap(node.next, _shards[cur_shard]._lru_head->next);
    _shards[cur_shard]._cache_size -= node.value.size();
    node.value = "";
    while (_shards[cur_shard]._max_size - _shards[cur_shard]._cache_size < value.size()) {
        _delete_least_recent(cur_shard);
    }
    _shards[cur_shard]._cache_size += value.size();
    node.value = value;
}

void StripedLockLRU::_delete_least_recent(size_t cur_shard) {
    auto node = _shards[cur_shard]._lru_head->prev;
    std::swap(node->prev, node->next->prev);
    std::swap(node->next, node->next->prev->next);
    _shards[cur_shard]._cache_size -= node->key.size() + node->value.size();
    _shards[cur_shard]._lru_index.erase(node->key);
    node->next.reset();
}

} // namespace Backend
} // namespace Afina
