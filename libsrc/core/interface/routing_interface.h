/*
 * * file name: routing_interface.h
 * * description: 选路插件接口（对齐ua_server的IRouting设计，简化版：
 * *              无version/zone/world_id参数，无广播接口）
 * */

#ifndef _APP_ROUTING_INTERFACE_H_
#define _APP_ROUTING_INTERFACE_H_

#include <cstddef>
#include <cstdint>

namespace app
{
class IRouting
{
public:
    /// 添加路由节点
    virtual void AddRoute(uint32_t svr_type, uint32_t node_id) = 0;
    /// 删除路由节点
    virtual void DelRoute(uint32_t svr_type, uint32_t node_id) = 0;
    /// 计算发送目标：如果路由表中有该svr_type的节点，按策略选路；
    /// 如果没有节点，返回expect_dest_id（降级为显式指定目标）
    virtual uint32_t GetSendDest(uint32_t svr_type, uint64_t gid, uint32_t expect_dest_id) const = 0;
    /// 获取指定svr_type的节点数
    virtual size_t GetNodeNum(uint32_t svr_type) const = 0;
    /// 清空所有路由信息
    virtual void Clear() = 0;

    virtual ~IRouting() = default;
};

}  // namespace app

#endif
