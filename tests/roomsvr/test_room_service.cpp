#include "room_service.h"

#include <cstdio>
#include <string>

#include "core/interface/codec_interface.h"
#include "room.pb.h"
#include "room_error.h"
#include "room_mgr.h"

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

class TestReadCodec : public app::ReadCodec
{
public:
    explicit TestReadCodec(const google::protobuf::Message& request) { request.SerializeToString(&body_); }

    uint32_t GetCmd() const override { return 0; }
    uint64_t GetGid() const override { return 0; }
    uint64_t GetSeqID() const override { return 1; }
    uint32_t GetSrc() const override { return 1; }
    uint32_t GetDst() const override { return 2; }
    uint32_t GetBodyLen() const override { return static_cast<uint32_t>(body_.size()); }
    const char* GetBody() const override { return body_.data(); }
    void Reset() override { body_.clear(); }

private:
    std::string body_;
};

int32_t CallPlayerSettle(const roomsvr::RoomDsPlayerSettleReq& request)
{
    TestReadCodec codec(request);
    app::RpcContext context(0, codec, &roomsvr::RoomDsPlayerSettleReq::default_instance(),
                            &roomsvr::RoomDsPlayerSettleResp::default_instance());
    roomsvr::RoomService::RoomDsPlayerSettle(context);
    return static_cast<const roomsvr::RoomDsPlayerSettleResp&>(context.GetRsp()).ret_code();
}

int32_t CallGameFinish(const roomsvr::RoomDsGameFinishReq& request)
{
    TestReadCodec codec(request);
    app::RpcContext context(0, codec, &roomsvr::RoomDsGameFinishReq::default_instance(),
                            &roomsvr::RoomDsGameFinishResp::default_instance());
    roomsvr::RoomService::RoomDsGameFinish(context);
    return static_cast<const roomsvr::RoomDsGameFinishResp&>(context.GetRsp()).ret_code();
}

int32_t CallNotifyDsTimeout(const roomsvr::NotifyDsTimeoutReq& request)
{
    TestReadCodec codec(request);
    app::RpcContext context(0, codec, &roomsvr::NotifyDsTimeoutReq::default_instance(),
                            &roomsvr::NotifyDsTimeoutResp::default_instance());
    roomsvr::RoomService::NotifyDsTimeout(context);
    return static_cast<const roomsvr::NotifyDsTimeoutResp&>(context.GetRsp()).ret_code();
}

roomsvr::RoomDsPlayerSettleReq MakeSettle(uint64_t room_id, uint64_t generation, uint64_t gid, bool is_bot,
                                          const std::string& bot_id, uint32_t kills)
{
    roomsvr::RoomDsPlayerSettleReq request;
    request.set_room_id(room_id);
    request.set_battle_generation(generation);
    auto* info = request.mutable_player_info();
    info->set_gid(gid);
    info->set_b_is_bot(is_bot);
    info->set_bot_id(bot_id);
    info->set_kills(kills);
    info->set_deaths(1);
    info->set_rank(1);
    return request;
}

int32_t CallSendEmote(uint64_t room_id, uint64_t gid, uint32_t emote_id)
{
    roomsvr::RoomSendEmoteReq request;
    request.set_room_id(room_id);
    request.set_gid(gid);
    request.set_emote_id(emote_id);
    TestReadCodec codec(request);
    app::RpcContext context(0, codec, &roomsvr::RoomSendEmoteReq::default_instance(),
                            &roomsvr::RoomSendEmoteResp::default_instance());
    roomsvr::RoomService::RoomSendEmote(context);
    return static_cast<const roomsvr::RoomSendEmoteResp&>(context.GetRsp()).ret_code();
}

}  // namespace

int main()
{
    roomsvr::Room* room = roomsvr::RoomMgr::GetInst().AddRoom(2001, "settle_test", 4);
    CHECK(room != nullptr, "create room");
    CHECK(room->map_id() == roomsvr::kDefaultMapId, "room defaults to map 101");
    CHECK(room->GetMember(2001) && room->GetMember(2001)->is_ready, "room creator is ready by default");
    CHECK(room->AddMember(2002, 2, false) == 0, "add second real player");
    CHECK(room->GetMember(2002) && !room->GetMember(2002)->is_ready, "joined player is not ready by default");
    CHECK(!room->AllReady(), "unready joined player blocks battle start");

    std::string bot_id;
    uint32_t bot_slot = 0;
    CHECK(room->AddBot(3, &bot_id, &bot_slot) == 0, "add bot");
    uint64_t generation = room->BeginBattleGeneration();
    room->SetInBattle("127.0.0.1:9000", "battle-test");
    room->SetInBattleState();

    // 表情：非Waiting状态一律回1003（先于emote_id校验），与RoomSetMap口径一致
    CHECK(CallSendEmote(room->room_id(), 2001, 3) == roomsvr::kRoomInBattle, "emote rejected while in battle");

    auto real_request = MakeSettle(room->room_id(), generation, 2001, false, "", 2);
    CHECK(CallPlayerSettle(real_request) == 0, "valid real-player settle succeeds");
    CHECK(room->settle_data().at(2001).kills() == 2, "real-player settle stored by gid");

    auto bot_request = MakeSettle(room->room_id(), generation, 0, true, bot_id, 5);
    CHECK(CallPlayerSettle(bot_request) == 0, "valid bot settle succeeds with gid zero");
    CHECK(room->bot_settle_data().at(bot_id).kills() == 5, "bot settle stored by bot_id");

    bot_request.mutable_player_info()->set_kills(6);
    CHECK(CallPlayerSettle(bot_request) == 0, "duplicate bot settle follows last-write-wins");
    CHECK(room->bot_settle_data().at(bot_id).kills() == 6, "duplicate bot settle updates same bot only");

    room->RemoveMember(2002);
    CHECK(CallPlayerSettle(MakeSettle(room->room_id(), generation, 2002, false, "", 3)) == 0,
          "settle after leaving still matches frozen battle roster");
    CHECK(room->settle_data().at(2002).kills() == 3, "departed player settlement stored");

    CHECK(CallPlayerSettle(MakeSettle(room->room_id(), generation, 9, true, bot_id, 1)) == roomsvr::kNotInRoom,
          "bot with nonzero gid rejected");
    CHECK(CallPlayerSettle(MakeSettle(room->room_id(), generation, 0, false, "", 1)) == roomsvr::kNotInRoom,
          "real player with zero gid rejected");
    CHECK(CallPlayerSettle(MakeSettle(room->room_id(), generation, 2002, false, bot_id, 1)) == roomsvr::kNotInRoom,
          "real player carrying bot_id rejected");
    CHECK(CallPlayerSettle(MakeSettle(room->room_id(), generation, 0, true, "unknown_bot", 1)) == roomsvr::kNotInRoom,
          "unknown bot_id rejected");
    CHECK(CallPlayerSettle(MakeSettle(room->room_id(), generation + 1, 2002, false, "", 1)) == 3,
          "settle generation mismatch rejected");
    CHECK(room->settle_data().at(2002).kills() == 3, "rejected settle does not overwrite cached data");

    roomsvr::RoomDsGameFinishReq finish_request;
    finish_request.set_room_id(room->room_id());
    finish_request.set_battle_generation(generation + 1);
    finish_request.set_duration_sec(60);
    finish_request.set_end_reason(0);
    CHECK(CallGameFinish(finish_request) == 3, "finish generation mismatch rejected");
    CHECK(room->state() == roomsvr::ROOM_STATE_IN_BATTLE, "rejected finish does not end current battle");

    roomsvr::RoomService::DoGuaranteedSettle(*room, 4);
    CHECK(room->state() == roomsvr::ROOM_STATE_WAITING, "guaranteed settle returns room to waiting");
    CHECK(room->last_match().players_size() == 3, "guaranteed settle includes frozen battle roster");
    CHECK(room->last_match().players(2).b_is_bot() && room->last_match().players(2).bot_id() == bot_id &&
              room->last_match().players(2).kills() == 6,
          "guaranteed settle preserves reported bot score");

    // 表情：房间已回Waiting，校验白名单与成员归属（成功路径会走广播RPC，需真实channel，不在单测覆盖）
    CHECK(CallSendEmote(room->room_id(), 2001, 0) == roomsvr::kInvalidEmoteId, "emote_id 0 rejected");
    CHECK(CallSendEmote(room->room_id(), 2001, 7) == roomsvr::kInvalidEmoteId, "emote_id 7 rejected");
    CHECK(CallSendEmote(room->room_id(), 2002, 3) == roomsvr::kNotInRoom, "emote from departed player rejected");
    CHECK(CallSendEmote(room->room_id(), 0, 3) == roomsvr::kNotInRoom, "emote with bot gid zero rejected");
    CHECK(CallSendEmote(999999, 2001, 3) == roomsvr::kRoomNotFound, "emote to unknown room rejected");

    roomsvr::NotifyDsTimeoutReq timeout_request;
    timeout_request.set_room_id(room->room_id());
    timeout_request.set_battle_generation(generation);
    timeout_request.set_pid(1234);
    CHECK(CallNotifyDsTimeout(timeout_request) == 0, "late DS timeout after settlement is ignored");
    CHECK(roomsvr::RoomMgr::GetInst().GetRoom(room->room_id()) == room && room->state() == roomsvr::ROOM_STATE_WAITING,
          "late DS timeout does not destroy settled room");

    roomsvr::RoomMgr::GetInst().FreeRoom(room->room_id());
    std::puts("ALL ROOM SERVICE TESTS PASSED");
    return 0;
}
