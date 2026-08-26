/*
 * * file name: test_tcaplus.cpp
 * * description: 独立tcaplus验证测试，直接使用SDK连接tcaplus集群。
 * *              向login和user_info表分别写入标志数据，读回验证，确认链路打通。
 * *              用法: test_tcaplus [conf_file]
 * *              默认读取 conf/tcaplus.conf
 * */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>

#include "tcaplus_service/tcaplus_service_log.h"
#include "tcaplus_service/tcaplus_server.h"
#include "tcaplus_service/tcaplus_service_request.h"
#include "tcaplus_service/tcaplus_service_response.h"
#include "tcaplus_service/tcaplus_service_record.h"
#include "table/tb_app_tcaplus.h"

#include "tlog/tlog.h"
#include "tloghelp/tlogload.h"

using namespace TcaplusService;

// ============================================================================
// 简易配置解析
// ============================================================================

struct TestConf
{
    int64_t app_id = 0;
    int32_t zone_id = 0;
    std::string signature;
    std::vector<std::string> dir_url;
    std::vector<std::string> table_names;
};

static bool ParseConf(const std::string& path, TestConf& conf)
{
    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        fprintf(stderr, "ERROR: cannot open conf file: %s\n", path.c_str());
        return false;
    }

    std::string line;
    while (std::getline(ifs, line))
    {
        if (line.empty() || line[0] == '#')
            continue;
        auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        while (!key.empty() && (key.back() == ' ' || key.back() == '\t'))
            key.pop_back();
        while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r'))
            value.pop_back();

        if (key == "app_id")
            conf.app_id = atoll(value.c_str());
        else if (key == "zone_id")
            conf.zone_id = atoi(value.c_str());
        else if (key == "signature")
            conf.signature = value;
        else if (key == "dir_url")
            conf.dir_url.push_back(value);
        else if (key == "table_name")
            conf.table_names.push_back(value);
    }

    if (conf.app_id == 0 || conf.dir_url.empty() || conf.table_names.empty())
    {
        fprintf(stderr, "ERROR: conf missing required fields\n");
        return false;
    }

    printf("conf loaded: app_id=%ld, zone_id=%d, tables=%zu, dir_url_count=%zu\n",
           conf.app_id, conf.zone_id, conf.table_names.size(), conf.dir_url.size());
    return true;
}

// ============================================================================
// tcaplus操作
// ============================================================================

extern "C" unsigned char g_szMetalib_tcaplus_tb[];

static int PollResponse(TcaplusServer& svr, int timeout_sec)
{
    for (int i = 0; i < timeout_sec * 100; i++)
    {
        svr.OnUpdate();

        TcaplusServiceResponse* response = nullptr;
        int ret = svr.RecvResponse(response);
        if (ret > 0 && response)
        {
            return 0;  // got response
        }
        else if (ret < 0)
        {
            fprintf(stderr, "ERROR: RecvResponse fail, ret=%d\n", ret);
            return ret;
        }

        usleep(10000);
    }

    fprintf(stderr, "ERROR: poll response timeout (%d sec)\n", timeout_sec);
    return -1;
}

static int TestLoginTable(TcaplusServer& svr)
{
    const char* table = "login";
    const char* uid = "test_user_verify_001";
    const uint32_t login_flag = 88888;

    printf("\n--- Testing table: login ---\n");

    // REPLACE
    {
        TcaplusServiceRequest* request = svr.GetRequest(table);
        if (!request) { fprintf(stderr, "ERROR: GetRequest(%s) failed\n", table); return -1; }

        int ret = request->Init(TCAPLUS_API_REPLACE_REQ);
        if (ret < 0) { fprintf(stderr, "ERROR: Init fail, ret=%d\n", ret); return -1; }
        request->SetAsyncID(1);

        TcaplusServiceRecord* record = request->AddRecord();
        if (!record) { fprintf(stderr, "ERROR: AddRecord failed\n"); return -1; }

        LOGIN tb = {};
        strncpy(tb.szUid, uid, sizeof(tb.szUid) - 1);
        tb.dwLogin_flag = login_flag;

        ret = record->SetData(reinterpret_cast<const char*>(&tb), sizeof(tb));
        if (ret < 0) { fprintf(stderr, "ERROR: SetData fail\n"); return -1; }

        ret = svr.SendRequest(request);
        if (ret < 0) { fprintf(stderr, "ERROR: SendRequest fail\n"); return -1; }

        printf("  REPLACE: uid=%s, login_flag=%u\n", uid, login_flag);
    }

    if (PollResponse(svr, 10) != 0) return -1;

    // GET
    {
        TcaplusServiceRequest* request = svr.GetRequest(table);
        if (!request) { fprintf(stderr, "ERROR: GetRequest(%s) failed\n", table); return -1; }

        int ret = request->Init(TCAPLUS_API_GET_REQ);
        if (ret < 0) { fprintf(stderr, "ERROR: Init fail\n"); return -1; }
        request->SetAsyncID(2);

        TcaplusServiceRecord* record = request->AddRecord();
        if (!record) { fprintf(stderr, "ERROR: AddRecord failed\n"); return -1; }

        LOGIN tb = {};
        strncpy(tb.szUid, uid, sizeof(tb.szUid) - 1);
        ret = record->SetData(reinterpret_cast<const char*>(&tb), sizeof(tb));
        if (ret < 0) { fprintf(stderr, "ERROR: SetData fail\n"); return -1; }

        ret = svr.SendRequest(request);
        if (ret < 0) { fprintf(stderr, "ERROR: SendRequest fail\n"); return -1; }

        printf("  GET: uid=%s\n", uid);
    }

    // Poll and parse GET response
    for (int i = 0; i < 1000; i++)
    {
        svr.OnUpdate();
        TcaplusServiceResponse* response = nullptr;
        int ret = svr.RecvResponse(response);
        if (ret > 0 && response)
        {
            if (response->GetResult() != 0)
            {
                fprintf(stderr, "  ERROR: result=%d\n", response->GetResult());
                return -1;
            }
            if (response->GetRecordCount() > 0)
            {
                const TcaplusServiceRecord* rec = nullptr;
                response->FetchRecord(rec);
                if (rec)
                {
                    LOGIN tb = {};
                    int32_t dv = 0;
                    rec->GetData(&tb, sizeof(tb), &dv);
                    printf("  GET result: uid=%s, login_flag=%u, data_version=%d\n",
                           tb.szUid, tb.dwLogin_flag, dv);

                    if (strcmp(tb.szUid, uid) == 0 && tb.dwLogin_flag == login_flag)
                    {
                        printf("  login table VERIFIED!\n");
                        return 0;
                    }
                }
            }
            fprintf(stderr, "  login table VERIFY FAILED!\n");
            return -1;
        }
        else if (ret < 0)
        {
            return -1;
        }
        usleep(10000);
    }

    fprintf(stderr, "  login table GET timeout\n");
    return -1;
}

static int TestUserInfoTable(TcaplusServer& svr)
{
    const char* table = "user_info";
    const char* uid = "test_user_verify_001";
    const uint32_t role_type = 3;
    const char* user_name = "TestPlayer";
    const uint64_t points = 123456;

    printf("\n--- Testing table: user_info ---\n");

    // REPLACE
    {
        TcaplusServiceRequest* request = svr.GetRequest(table);
        if (!request) { fprintf(stderr, "ERROR: GetRequest(%s) failed\n", table); return -1; }

        int ret = request->Init(TCAPLUS_API_REPLACE_REQ);
        if (ret < 0) { fprintf(stderr, "ERROR: Init fail, ret=%d\n", ret); return -1; }
        request->SetAsyncID(3);

        TcaplusServiceRecord* record = request->AddRecord();
        if (!record) { fprintf(stderr, "ERROR: AddRecord failed\n"); return -1; }

        USER_INFO tb = {};
        strncpy(tb.szUid, uid, sizeof(tb.szUid) - 1);
        tb.dwRole_type = role_type;
        strncpy(tb.szUser_name, user_name, sizeof(tb.szUser_name) - 1);
        tb.ullPoints = points;

        ret = record->SetData(reinterpret_cast<const char*>(&tb), sizeof(tb));
        if (ret < 0) { fprintf(stderr, "ERROR: SetData fail\n"); return -1; }

        ret = svr.SendRequest(request);
        if (ret < 0) { fprintf(stderr, "ERROR: SendRequest fail\n"); return -1; }

        printf("  REPLACE: uid=%s, role_type=%u, user_name=%s, points=%lu\n",
               uid, role_type, user_name, (unsigned long)points);
    }

    if (PollResponse(svr, 10) != 0) return -1;

    // GET
    {
        TcaplusServiceRequest* request = svr.GetRequest(table);
        if (!request) { fprintf(stderr, "ERROR: GetRequest(%s) failed\n", table); return -1; }

        int ret = request->Init(TCAPLUS_API_GET_REQ);
        if (ret < 0) { fprintf(stderr, "ERROR: Init fail\n"); return -1; }
        request->SetAsyncID(4);

        TcaplusServiceRecord* record = request->AddRecord();
        if (!record) { fprintf(stderr, "ERROR: AddRecord failed\n"); return -1; }

        USER_INFO tb = {};
        strncpy(tb.szUid, uid, sizeof(tb.szUid) - 1);
        ret = record->SetData(reinterpret_cast<const char*>(&tb), sizeof(tb));
        if (ret < 0) { fprintf(stderr, "ERROR: SetData fail\n"); return -1; }

        ret = svr.SendRequest(request);
        if (ret < 0) { fprintf(stderr, "ERROR: SendRequest fail\n"); return -1; }

        printf("  GET: uid=%s\n", uid);
    }

    // Poll and parse GET response
    for (int i = 0; i < 1000; i++)
    {
        svr.OnUpdate();
        TcaplusServiceResponse* response = nullptr;
        int ret = svr.RecvResponse(response);
        if (ret > 0 && response)
        {
            if (response->GetResult() != 0)
            {
                fprintf(stderr, "  ERROR: result=%d\n", response->GetResult());
                return -1;
            }
            if (response->GetRecordCount() > 0)
            {
                const TcaplusServiceRecord* rec = nullptr;
                response->FetchRecord(rec);
                if (rec)
                {
                    USER_INFO tb = {};
                    int32_t dv = 0;
                    rec->GetData(&tb, sizeof(tb), &dv);
                    printf("  GET result: uid=%s, role_type=%u, user_name=%s, points=%lu, data_version=%d\n",
                           tb.szUid, tb.dwRole_type, tb.szUser_name, (unsigned long)tb.ullPoints, dv);

                    if (strcmp(tb.szUid, uid) == 0 &&
                        tb.dwRole_type == role_type &&
                        strcmp(tb.szUser_name, user_name) == 0 &&
                        tb.ullPoints == points)
                    {
                        printf("  user_info table VERIFIED!\n");
                        return 0;
                    }
                }
            }
            fprintf(stderr, "  user_info table VERIFY FAILED!\n");
            return -1;
        }
        else if (ret < 0)
        {
            return -1;
        }
        usleep(10000);
    }

    fprintf(stderr, "  user_info table GET timeout\n");
    return -1;
}

int main(int argc, char* argv[])
{
    std::string conf_path = (argc >= 2) ? argv[1] : "conf/tcaplus.conf";

    printf("=== tcaplus standalone verification test ===\n");
    printf("conf: %s\n", conf_path.c_str());

    TestConf conf;
    if (!ParseConf(conf_path, conf))
        return -1;

    // 初始化tlog
    LPTLOGCTX log_ctx = tlog_init_file_ctx_ex("test_tcaplus", TLOG_PRIORITY_TRACE,
                                                "/tmp/test_tcaplus.%Y%m%d.log", 3,
                                                50 * 1024 * 1024, 1, nullptr);
    LPTLOGCATEGORYINST category = nullptr;
    if (log_ctx)
        category = tlog_get_category(log_ctx, "test_tcaplus");

    // 初始化TLogger + TcaplusServer
    TLogger logger(category);
    TcaplusServer svr;
    int ret = svr.Init(&logger, 0, conf.app_id, conf.zone_id, conf.signature.c_str());
    if (ret < 0)
    {
        fprintf(stderr, "ERROR: TcaplusServer Init fail, ret=%d\n", ret);
        return -1;
    }
    svr.SetCheckHeartbeatInterval(120);

    for (const auto& url : conf.dir_url)
    {
        ret = svr.AddDirServerAddress(url.c_str());
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: AddDirServerAddress(%s) fail\n", url.c_str());
            return -1;
        }
    }

    // 注册所有表
    for (const auto& name : conf.table_names)
    {
        LPTDRMETA meta = tdr_get_meta_by_name((LPTDRMETALIB)g_szMetalib_tcaplus_tb, name.c_str());
        if (!meta)
        {
            fprintf(stderr, "ERROR: tdr_get_meta_by_name(%s) fail\n", name.c_str());
            return -1;
        }
        ret = svr.RegistTable(name.c_str(), meta, 10000);
        if (ret < 0)
        {
            fprintf(stderr, "ERROR: RegistTable(%s) fail\n", name.c_str());
            return -1;
        }
        printf("  registered table: %s\n", name.c_str());
    }

    printf("\nconnecting to tcaplus cluster...\n");
    ret = svr.ConnectAll(10000, 0);
    if (ret != 0)
    {
        fprintf(stderr, "ERROR: ConnectAll fail, ret=%d\n", ret);
        return -1;
    }
    printf("connected!\n");

    // 测试各表
    bool all_pass = true;

    if (TestLoginTable(svr) != 0)
        all_pass = false;

    if (TestUserInfoTable(svr) != 0)
        all_pass = false;

    svr.Fini();

    if (all_pass)
    {
        printf("\n=== ALL TABLES VERIFICATION PASSED ===\n");
        return 0;
    }
    else
    {
        fprintf(stderr, "\n=== SOME TABLES VERIFICATION FAILED ===\n");
        return -1;
    }
}
