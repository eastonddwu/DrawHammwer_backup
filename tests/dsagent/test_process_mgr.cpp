#include "process_mgr.h"

#include <unistd.h>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "common/clock.h"
#include "dsa_app.h"

namespace
{

#define CHECK(condition, message)                                            \
    do                                                                       \
    {                                                                        \
        if (!(condition))                                                    \
        {                                                                    \
            std::fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); \
            return 1;                                                        \
        }                                                                    \
    } while (false)

int RunHelper()
{
    const char* behavior = std::getenv("TEST_DS_BEHAVIOR");
    if (behavior && std::strcmp(behavior, "self_exit") == 0)
    {
        usleep(200000);
        return 0;
    }
    if (behavior && std::strcmp(behavior, "ignore_term") == 0)
        std::signal(SIGTERM, SIG_IGN);

    while (true)
        pause();
}

std::string SelfPath()
{
    char path[1024] = {};
    ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
    if (len <= 0)
        return {};
    path[len] = '\0';
    return path;
}

void TickAfter(uint64_t delta_ms)
{
    dsagent::ProcessMgr::GetInst().OnTick(app::Clock::GetInst().CurrentMilliSec() + delta_ms);
}

}  // namespace

int main(int argc, char* argv[])
{
    if (argc > 1 && std::strcmp(argv[1], "DrawHammer") == 0)
        return RunHelper();

    std::string self_path = SelfPath();
    CHECK(!self_path.empty(), "resolve test executable path");
    dsagent::DsaApp::GetInst().Setup("", 19000, 25000, 25020, "127.0.0.1", "127.0.0.1", self_path, "ue_ds");
    dsagent::ProcessMgr::GetInst().SetPortRange(25000, 25020);

    setenv("TEST_DS_BEHAVIOR", "self_exit", 1);
    CHECK(dsagent::ProcessMgr::GetInst().CreateDS(9001, 1, 25000) == 0, "create self-exit helper");
    usleep(100000);
    CHECK(dsagent::ProcessMgr::GetInst().DestroyDS(9001, 1, 0) == 0, "disconnect destroy waits for self exit");
    const dsagent::DSProcess* normal = dsagent::ProcessMgr::GetInst().GetDS(9001);
    CHECK(normal && normal->state == dsagent::DS_STATE_WAIT_SELF_EXIT, "normal destroy waits for self exit");
    usleep(300000);
    TickAfter(1000);
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9001) == nullptr, "self-exited process cleaned without signal");

    setenv("TEST_DS_BEHAVIOR", "ignore_term", 1);
    CHECK(dsagent::ProcessMgr::GetInst().CreateDS(9002, 2, 25001) == 0, "create SIGTERM-ignoring helper");
    usleep(100000);
    CHECK(dsagent::ProcessMgr::GetInst().DestroyDS(9002, 3, 3) == 2, "wrong generation rejected");
    CHECK(!dsagent::ProcessMgr::GetInst().GetDS(9002)->destroy_requested, "wrong generation does not stop DS");
    CHECK(dsagent::ProcessMgr::GetInst().DestroyDS(9002, 2, 3) == 0, "accept abnormal destroy request");
    const dsagent::DSProcess* stopping = dsagent::ProcessMgr::GetInst().GetDS(9002);
    CHECK(stopping && stopping->state == dsagent::DS_STATE_TERM_SENT, "abnormal destroy sends SIGTERM first");
    uint64_t deadline = stopping->stop_deadline_ms;
    CHECK(dsagent::ProcessMgr::GetInst().DestroyDS(9002, 2, 3) == 0, "duplicate destroy is idempotent");
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9002)->stop_deadline_ms == deadline,
          "duplicate destroy does not extend deadline");

    TickAfter(dsagent::ProcessMgr::kTermGraceMs + 1000);
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9002)->state == dsagent::DS_STATE_KILL_SENT,
          "SIGTERM timeout escalates to SIGKILL");
    usleep(100000);
    TickAfter(dsagent::ProcessMgr::kTermGraceMs + 2000);
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9002) == nullptr, "SIGKILL cleanup waits until process group gone");
    CHECK(dsagent::ProcessMgr::GetInst().DestroyDS(9002, 2, 3) == 0, "destroy after cleanup remains idempotent");

    setenv("TEST_DS_BEHAVIOR", "ignore_term", 1);
    CHECK(dsagent::ProcessMgr::GetInst().CreateDS(9003, 3, 25002) == 0, "create normal-stop timeout helper");
    usleep(100000);
    CHECK(dsagent::ProcessMgr::GetInst().DestroyDS(9003, 3, 2) == 0, "normal stop enters self-exit grace");
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9003)->state == dsagent::DS_STATE_WAIT_SELF_EXIT,
          "normal stop sends no signal before grace expires");
    TickAfter(dsagent::ProcessMgr::kSelfExitGraceMs + 1000);
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9003)->state == dsagent::DS_STATE_TERM_SENT,
          "normal self-exit timeout escalates to SIGTERM");
    TickAfter(dsagent::ProcessMgr::kSelfExitGraceMs + dsagent::ProcessMgr::kTermGraceMs + 2000);
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9003)->state == dsagent::DS_STATE_KILL_SENT,
          "normal SIGTERM timeout escalates to SIGKILL");
    usleep(100000);
    TickAfter(dsagent::ProcessMgr::kSelfExitGraceMs + dsagent::ProcessMgr::kTermGraceMs + 3000);
    CHECK(dsagent::ProcessMgr::GetInst().GetDS(9003) == nullptr, "normal timeout fallback eventually cleans process");
    CHECK(dsagent::ProcessMgr::GetInst().GetDSCount() == 0, "all test DS mappings cleaned");

    std::puts("ALL PROCESS MGR TESTS PASSED");
    return 0;
}
