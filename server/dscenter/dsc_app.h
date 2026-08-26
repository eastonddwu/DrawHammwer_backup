/*
 * * file name: dsc_app.h
 * * description: dscenter的业务server，管理DSAgent负载均衡分配
 */

#ifndef _DSC_APP_H_
#define _DSC_APP_H_

#include <cstdint>
#include <string>
#include "patterns/singleton.h"
#include "svr_base/base_server.h"

namespace dscenter
{
class DscApp : public app::BaseServer, public app::Singleton<DscApp>
{
public:
    void Setup(const std::string& tbus2_agent_url);

    static constexpr uint32_t kDscGroupBase = 0x07000000;

protected:
    virtual bool OnInit() override;
    virtual void OnTick(uint64_t now_ms, uint64_t tick_count) override;

private:
    friend class app::Singleton<DscApp>;
    DscApp() = default;

    std::string tbus2_agent_url_;
};

}  // namespace dscenter

#endif
