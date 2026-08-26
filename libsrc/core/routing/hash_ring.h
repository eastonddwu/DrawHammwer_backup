/*
 * * file name: hash_ring.h
 * * description: 一致性哈希环（参考ua_server的Consistent组件简化实现），
 * *              用于RoutingMgr的ConsistentHash策略。支持虚拟节点、动态增删、
 * *              FNV-1a哈希函数。
 * */

#ifndef _APP_HASH_RING_H_
#define _APP_HASH_RING_H_

#include <cstdint>
#include <map>
#include <vector>

namespace app
{

/// FNV-1a 哈希（64-bit）
inline uint64_t Fnv1aHash64(const char* data, size_t len)
{
    uint64_t hash = 14695981039346656037ULL;  // FNV offset basis
    for (size_t i = 0; i < len; ++i)
    {
        hash ^= static_cast<uint64_t>(static_cast<uint8_t>(data[i]));
        hash *= 1099511628211ULL;  // FNV prime
    }
    return hash;
}

/// 一致性哈希环
class HashRing
{
public:
    static constexpr uint32_t kDefaultVirtualNodes = 100;

    explicit HashRing(uint32_t virtual_nodes = kDefaultVirtualNodes)
        : virtual_nodes_(virtual_nodes)
    {
    }

    /// 添加一个真实节点（自动添加virtual_nodes_个虚拟节点）
    void AddNode(uint32_t node_id)
    {
        for (uint32_t i = 0; i < virtual_nodes_; ++i)
        {
            // 虚拟节点key: hash(node_id << 32 | replica_index)
            uint64_t vnode_key = (static_cast<uint64_t>(node_id) << 32) | i;
            auto vnode_bytes = reinterpret_cast<const char*>(&vnode_key);
            uint64_t hash = Fnv1aHash64(vnode_bytes, sizeof(vnode_key));

            // 哈希冲突时保留较小的node_id
            auto iter = ring_.find(hash);
            if (iter != ring_.end())
            {
                if (node_id < iter->second)
                    iter->second = node_id;
            }
            else
            {
                ring_[hash] = node_id;
            }
        }
        nodes_.push_back(node_id);
    }

    /// 移除一个真实节点
    void RemoveNode(uint32_t node_id)
    {
        for (uint32_t i = 0; i < virtual_nodes_; ++i)
        {
            uint64_t vnode_key = (static_cast<uint64_t>(node_id) << 32) | i;
            auto vnode_bytes = reinterpret_cast<const char*>(&vnode_key);
            uint64_t hash = Fnv1aHash64(vnode_bytes, sizeof(vnode_key));

            auto iter = ring_.find(hash);
            if (iter != ring_.end() && iter->second == node_id)
            {
                ring_.erase(iter);
            }
        }
        // swap-and-pop from nodes_
        for (size_t i = 0; i < nodes_.size(); ++i)
        {
            if (nodes_[i] == node_id)
            {
                nodes_[i] = nodes_.back();
                nodes_.pop_back();
                break;
            }
        }
    }

    /// 按数据key查找归属节点，返回node_id。环为空时返回0。
    uint32_t GetNode(uint64_t data_key) const
    {
        if (ring_.empty())
            return 0;

        // 对data_key的原始字节做FNV-1a哈希（与AddNode里虚拟节点key的哈希方式一致，
        // 避免每次查找都为std::to_string分配临时字符串。虚拟节点key与数据key是
        // 各自独立的哈希输入，只要都落到同一uint64环空间即可正确路由）。
        auto key_bytes = reinterpret_cast<const char*>(&data_key);
        uint64_t hash = Fnv1aHash64(key_bytes, sizeof(data_key));

        auto iter = ring_.lower_bound(hash);
        if (iter == ring_.end())
            iter = ring_.begin();  // 环绕

        return iter->second;
    }

    /// 判断环是否为空
    bool Empty() const { return ring_.empty(); }

    /// 获取真实节点数
    size_t NodeCount() const { return nodes_.size(); }

private:
    uint32_t virtual_nodes_;
    /// hash → node_id 的有序映射（哈希环）
    std::map<uint64_t, uint32_t> ring_;
    /// 真实节点列表
    std::vector<uint32_t> nodes_;
};

}  // namespace app

#endif
