#include "room.h"

#include <cstdio>
#include <string>

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

}  // namespace

int main()
{
    roomsvr::Room room(1, 1001, "test_room", 4);
    CHECK(room.AddMember(1001, 1, false, "甲玩家") == 0, "add host");
    CHECK(room.AddMember(1002, 2, false, "乙玩家") == 0, "add second player");

    const roomsvr::RoomMemberData* host = room.GetMember(1001);
    const roomsvr::RoomMemberData* second = room.GetMember(1002);
    CHECK(host && host->battle_role_type == roomsvr::kDefaultBattleRoleType, "host default role");
    CHECK(second && second->battle_role_type == roomsvr::kDefaultBattleRoleType, "joined player default role");
    CHECK(host && host->display_name == "甲玩家" && second->display_name == "乙玩家",
          "real member display names preserved");
    CHECK(!host->is_ready && !second->is_ready, "real players initially not ready");
    CHECK(!room.AllReady(), "host and second player participate in ready check");

    CHECK(room.SetReady(1001, true), "set host ready");
    CHECK(!room.AllReady(), "second player still blocks AllReady");
    CHECK(room.SetReady(1002, true), "set second player ready");
    CHECK(room.AllReady(), "all real players ready");
    CHECK(room.map_id() == roomsvr::kDefaultMapId, "room defaults to living room map");
    CHECK(!room.SetMap(roomsvr::kDefaultMapId), "same map is a no-op");
    CHECK(room.GetMember(1001)->is_ready && room.GetMember(1002)->is_ready, "same map keeps ready state");
    CHECK(room.SetMap(roomsvr::kPoolMapId), "switch to pool map");
    CHECK(room.map_id() == roomsvr::kPoolMapId, "pool map stored");
    CHECK(room.GetMember(1001)->is_ready && !room.GetMember(1002)->is_ready,
          "map switch keeps host ready and clears joined player ready");
    CHECK(room.SetBattleRole(1001, 2), "ready host can still select role after map switch");
    CHECK(room.SetBattleRole(1001, roomsvr::kDefaultBattleRoleType), "restore host role for remaining test");
    CHECK(room.SetReady(1001, true), "host remains ready after map switch");
    CHECK(room.SetReady(1002, true), "second player can ready after map switch");
    CHECK(room.AllReady(), "all real players ready after map switch");
    CHECK(room.SetReady(1001, false), "host can cancel ready");
    CHECK(!room.AllReady(), "host cancel ready blocks AllReady");
    CHECK(room.SetReady(1001, true), "host can ready again");

    std::string bot_id;
    uint32_t bot_slot = 0;
    CHECK(room.AddBot(3, &bot_id, &bot_slot) == 0, "add bot");
    roomsvr::RoomMemberData* bot = room.GetBot(bot_id);
    CHECK(bot && bot->is_ready, "bot always ready");
    CHECK(bot->battle_role_type == roomsvr::kDefaultBattleRoleType, "bot default role");
    CHECK(room.AllReady(), "bot does not block AllReady");

    CHECK(room.SetBattleRole(1002, 0), "inject legacy zero role");
    bot->battle_role_type = 7;
    room.FinalizeBattleRoles();
    CHECK(room.GetMember(1002)->battle_role_type == roomsvr::kDefaultBattleRoleType, "zero role falls back to default");
    CHECK(room.GetBot(bot_id)->battle_role_type == 7, "nonzero bot role is preserved");

    std::string bot_id_2;
    uint32_t bot_slot_2 = 0;
    CHECK(room.AddBot(4, &bot_id_2, &bot_slot_2) == 0, "add second bot");

    roomsvr::FightPlayerInfo host_settle;
    host_settle.set_gid(1001);
    host_settle.set_kills(2);
    host_settle.set_deaths(3);
    host_settle.set_rank(2);
    host_settle.set_display_name("host");
    room.StoreSettleData(1001, host_settle);

    roomsvr::FightPlayerInfo bot_settle;
    bot_settle.set_b_is_bot(true);
    bot_settle.set_bot_id(bot_id);
    bot_settle.set_kills(5);
    bot_settle.set_deaths(1);
    bot_settle.set_rank(1);
    room.StoreBotSettleData(bot_id, bot_settle);

    roomsvr::FightPlayerInfo bot_settle_2;
    bot_settle_2.set_b_is_bot(true);
    bot_settle_2.set_bot_id(bot_id_2);
    bot_settle_2.set_kills(1);
    bot_settle_2.set_deaths(4);
    bot_settle_2.set_rank(3);
    room.StoreBotSettleData(bot_id_2, bot_settle_2);

    roomsvr::MatchSummary summary = room.BuildMatchSummary(90, 0);
    CHECK(summary.players_size() == 4, "summary contains every room member exactly once");
    CHECK(summary.players(0).gid() == 1001 && summary.players(0).kills() == 2 && summary.players(0).rank() == 2,
          "real player settlement preserved");
    CHECK(summary.players(1).gid() == 1002 && summary.players(1).kills() == 0 && summary.players(1).rank() == 0,
          "missing real settlement falls back to zero");
    CHECK(summary.players(2).b_is_bot() && summary.players(2).bot_id() == bot_id && summary.players(2).kills() == 5 &&
              summary.players(2).rank() == 1,
          "first bot settlement preserved without rank reorder");
    CHECK(summary.players(3).b_is_bot() && summary.players(3).bot_id() == bot_id_2 && summary.players(3).kills() == 1 &&
              summary.players(3).rank() == 3,
          "second bot settlement remains independent despite gid zero");

    uint32_t host_slot = room.GetMember(1001)->slot_index;
    CHECK(room.BeginBattleGeneration() == 1, "first battle generation");
    CHECK(room.BeginBattleGeneration() == 2, "battle generation increments");
    room.SetInBattle("127.0.0.1:9000", "battle-1");
    room.SetInBattleState();
    room.SetPlayerToken(1001, 123);
    room.ClearBattleState();

    CHECK(room.state() == roomsvr::ROOM_STATE_WAITING, "battle returns to waiting");
    CHECK(room.map_id() == roomsvr::kPoolMapId, "battle cleanup preserves map");
    CHECK(room.GetMember(1001)->display_name == "甲玩家" && room.GetMember(1002)->display_name == "乙玩家",
          "battle cleanup preserves display names");
    CHECK(!room.in_battle(), "in_battle cleared");
    CHECK(room.GetMember(1001)->is_ready, "battle cleanup keeps host ready");
    CHECK(!room.GetMember(1002)->is_ready, "battle cleanup clears joined player ready");
    CHECK(room.GetMember(1001)->battle_role_type == roomsvr::kDefaultBattleRoleType, "host role preserved");
    CHECK(room.GetMember(1002)->battle_role_type == roomsvr::kDefaultBattleRoleType, "second role preserved");
    CHECK(room.GetBot(bot_id)->is_ready && room.GetBot(bot_id)->battle_role_type == 7, "bot state preserved");
    CHECK(room.GetMember(1001)->slot_index == host_slot && room.GetBot(bot_id)->slot_index == bot_slot,
          "fixed slots preserved");
    CHECK(room.settle_data().empty() && room.bot_settle_data().empty(), "battle cleanup clears both settlement maps");
    CHECK(!room.HasPlayerToken(1001), "battle cleanup clears player tokens");

    roomsvr::Room departure_room(2, 3001, "departure_test", 2);
    CHECK(departure_room.AddMember(3001, 1, false) == 0, "add departure host");
    CHECK(departure_room.AddMember(3002, 2, false) == 0, "add departing player");
    departure_room.BeginBattleGeneration();
    roomsvr::FightPlayerInfo departing_settle;
    departing_settle.set_gid(3002);
    departing_settle.set_kills(4);
    departure_room.StoreSettleData(3002, departing_settle);
    departure_room.RemoveMember(3002);
    roomsvr::MatchSummary departure_summary = departure_room.BuildMatchSummary(30, 0);
    CHECK(departure_summary.players_size() == 2 && departure_summary.players(1).gid() == 3002 &&
              departure_summary.players(1).kills() == 4,
          "frozen battle roster preserves settled player after leaving room");

    // 回归：同一房间连续两局。
    // 线上曾出现房主打完一局后点开始永远无反应——ClearBattleState()把房主is_ready一起清成false，
    // 而客户端只在首次进房点准备、结算后不会补发SetReady，AllReady()因此永远失败。
    // 建房时房主默认已准备(RoomMgr::AddRoom传is_ready=true)，这里用同样的方式构造。
    roomsvr::Room rematch_room(3, 5001, "rematch_test", 4);
    CHECK(rematch_room.AddMember(5001, 1, true, "房主") == 0, "add auto-ready host");
    std::string rematch_bot;
    CHECK(rematch_room.AddBot(2, &rematch_bot, nullptr) == 0, "add bot for rematch room");
    CHECK(rematch_room.real_player_count() == 1 && rematch_room.member_count() == 2,
          "rematch room has 1 real player and 1 bot");
    CHECK(rematch_room.AllReady(), "first battle can start");

    rematch_room.BeginBattleGeneration();
    rematch_room.SetInBattle("127.0.0.1:9100", "battle-r1");
    rematch_room.SetInBattleState();
    CHECK(rematch_room.state() == roomsvr::ROOM_STATE_IN_BATTLE, "rematch room entered battle");
    rematch_room.ClearBattleState();

    CHECK(rematch_room.state() == roomsvr::ROOM_STATE_WAITING, "settlement returns room to waiting");
    CHECK(rematch_room.GetMember(5001)->is_ready, "host stays ready after settlement");
    CHECK(rematch_room.GetBot(rematch_bot)->is_ready, "bot stays ready after settlement");
    CHECK(rematch_room.AllReady(), "second battle can start without re-sending SetReady");

    // 回归：房主战斗中掉线后房主迁移，新房主也必须是已准备态。
    // 结算流程是 ClearBattleState() -> ApplyPendingLeaves() -> MigrateHost()，
    // 迁移发生在准备态重置之后，若MigrateHost不补准备态，新房主会复现同一个卡死。
    roomsvr::Room migrate_room(4, 6001, "migrate_test", 4);
    CHECK(migrate_room.AddMember(6001, 1, true, "原房主") == 0, "add original host");
    CHECK(migrate_room.AddMember(6002, 2, false, "后加入者") == 0, "add second real player");
    std::string migrate_bot;
    CHECK(migrate_room.AddBot(3, &migrate_bot, nullptr) == 0, "add bot for migrate room");
    migrate_room.ClearBattleState();
    CHECK(!migrate_room.GetMember(6002)->is_ready, "non-host ready cleared by settlement");
    migrate_room.RemoveMember(6001);
    CHECK(migrate_room.MigrateHost() == 6002, "host migrates to earliest remaining real player");
    CHECK(migrate_room.GetMember(6002)->is_ready, "migrated host is marked ready");
    CHECK(migrate_room.AllReady(), "migrated host can start the next battle");

    std::puts("ALL ROOM TESTS PASSED");
    return 0;
}
