/*
 * * file name: routing_mgr.cpp
 * * description: RoutingMgr + ServiceRoute实现
 * */

#include "routing_mgr.h"
#include <algorithm>
#include "core/log.h"

namespace app
{

// ==================== ServiceRoute ====================

void ServiceRoute::AddNode(uint32_t node_id)
{
    // 去重
    if (HasNode(node_id))
        return;

    nodes_.push_back(node_id);

    if (policy_type_ == kPolicyConsistentHash)
    {
        hash_ring_.AddNode(node_id);
    }

    APP_LOG_INFO(0, "route add node, svr_type policy(%u), node_id(%u), total(%zu)",
                 static_cast<uint32_t>(policy_type_), node_id, nodes_.size());
}

void ServiceRoute::DelNode(uint32_t node_id)
{
    auto iter = std::find(nodes_.begin(), nodes_.end(), node_id);
    if (iter == nodes_.end())
        return;

    // swap-and-pop
    *iter = nodes_.back();
    nodes_.pop_back();

    if (policy_type_ == kPolicyConsistentHash)
    {
        hash_ring_.RemoveNode(node_id);
    }

    APP_LOG_INFO(0, "route del node, svr_type policy(%u), node_id(%u), remaining(%zu)",
                 static_cast<uint32_t>(policy_type_), node_id, nodes_.size());
}

uint32_t ServiceRoute::GetOne(uint64_t gid) const
{
    if (nodes_.empty())
        return 0;

    if (policy_type_ == kPolicyConsistentHash)
    {
        if (gid == 0 || hash_ring_.Empty())
            return RandomSelect();
        return hash_ring_.GetNode(gid);
    }

    // kPolicyRandom
    return RandomSelect();
}

bool ServiceRoute::HasNode(uint32_t node_id) const
{
    return std::find(nodes_.begin(), nodes_.end(), node_id) != nodes_.end();
}

uint32_t ServiceRoute::RandomSelect() const
{
    if (nodes_.empty())
        return 0;
    return nodes_[static_cast<size_t>(rand()) % nodes_.size()];
}

// ==================== RoutingMgr ====================

void RoutingMgr::AddRoute(uint32_t svr_type, uint32_t node_id)
{
    GetOrCreateRoute(svr_type).AddNode(node_id);
}

void RoutingMgr::DelRoute(uint32_t svr_type, uint32_t node_id)
{
    auto iter = svr_routes_.find(svr_type);
    if (iter != svr_routes_.end())
    {
        iter->second.DelNode(node_id);
    }
}

uint32_t RoutingMgr::GetSendDest(uint32_t svr_type, uint64_t gid, uint32_t expect_dest_id) const
{
    auto iter = svr_routes_.find(svr_type);
    if (iter == svr_routes_.end() || iter->second.NodeCount() == 0)
    {
        // 该svr_type没有路由节点，降级使用显式指定的目标
        return expect_dest_id;
    }

    uint32_t dest = iter->second.GetOne(gid);
    if (dest == 0)
        return expect_dest_id;

    return dest;
}

size_t RoutingMgr::GetNodeNum(uint32_t svr_type) const
{
    auto iter = svr_routes_.find(svr_type);
    if (iter == svr_routes_.end())
        return 0;
    return iter->second.NodeCount();
}

void RoutingMgr::Clear()
{
    svr_routes_.clear();
}

ServiceRoute& RoutingMgr::GetOrCreateRoute(uint32_t svr_type)
{
    auto iter = svr_routes_.find(svr_type);
    if (iter != svr_routes_.end())
        return iter->second;

    // 默认使用ConsistentHash策略
    auto result = svr_routes_.emplace(svr_type, ServiceRoute(kPolicyConsistentHash));
    return result.first->second;
}

}  // namespace app
