/*
 * * file name: routing_mgr.h
 * * description: 路由管理器（对齐ua_server的RoutingMgr设计，简化版），
 * *              管理svr_type → ServiceRoute映射，支持ConsistentHash和Random策略。
 * *              通过tbus2 mesh事件动态增删路由节点，框架层TransportInfo::Send()
 * *              自动查路由替换dst。
 * */

#ifndef _APP_ROUTING_MGR_H_
#define _APP_ROUTING_MGR_H_

#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <vector>
#include "core/interface/routing_interface.h"
#include "core/routing/hash_ring.h"
#include "patterns/singleton.h"

namespace app
{

/// 路由策略类型
enum PolicyType : uint32_t
{
    kPolicyConsistentHash = 0,  // 默认：一致性哈希（同一gid总是路由到同一节点）
    kPolicyRandom = 1,          // 随机选择
};

/// 单个服务类型的路由表
class ServiceRoute
{
public:
    explicit ServiceRoute(PolicyType policy_type = kPolicyConsistentHash)
        : policy_type_(policy_type)
    {
    }

    void AddNode(uint32_t node_id);
    void DelNode(uint32_t node_id);
    uint32_t GetOne(uint64_t gid) const;
    size_t NodeCount() const { return nodes_.size(); }
    bool HasNode(uint32_t node_id) const;
    const std::vector<uint32_t>& GetAllNodes() const { return nodes_; }

private:
    /// 随机选择一个节点（gid==0或Random策略时使用）
    uint32_t RandomSelect() const;

    PolicyType policy_type_;
    /// 真实节点列表（用于随机选择和快速查找）
    std::vector<uint32_t> nodes_;
    /// 一致性哈希环（kPolicyConsistentHash时使用）
    HashRing hash_ring_;
};

/// 路由管理器（单例）
class RoutingMgr : public IRouting, public Singleton<RoutingMgr>
{
public:
    /// IRouting接口实现
    void AddRoute(uint32_t svr_type, uint32_t node_id) override;
    void DelRoute(uint32_t svr_type, uint32_t node_id) override;
    uint32_t GetSendDest(uint32_t svr_type, uint64_t gid, uint32_t expect_dest_id) const override;
    size_t GetNodeNum(uint32_t svr_type) const override;
    void Clear() override;

private:
    friend class Singleton<RoutingMgr>;
    RoutingMgr() = default;

    /// 获取或创建指定svr_type的ServiceRoute
    ServiceRoute& GetOrCreateRoute(uint32_t svr_type);

    /// svr_type → ServiceRoute映射
    std::unordered_map<uint32_t, ServiceRoute> svr_routes_;
};

}  // namespace app

#endif
