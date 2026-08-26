// Copyright (c) Tencent
// Author: yechangwu, bondshi
// Create: 2025-08-11
// Note: The features provided in this header requires C++14 or above

#ifndef TBUSPP2_INC_CPP_ENDPOINT_HPP_
#define TBUSPP2_INC_CPP_ENDPOINT_HPP_

#include <assert.h>
#include <string>
#include <utility>
#include <vector>
#include <functional>
#include "tbuspp2.h"

// This file provides C++ wrappers around the C API defined in tbuspp2.h.
// Each wrapper function is a thin layer that may convert exceptions or
// provide a more C++-friendly interface. For detailed documentation of the
// underlying functions, refer to the comments in tbuspp2.h.
//
// Common parameters:
//   - wait_ms: Operation timeout (ms). 0=non-blocking, <0=infinite, >0=wait time.
//   - ctx:     Optional context (nullable), typically for async operations.
//
// You could inherit EndpointWrap class to add more tbuspp2 API function wrap
//

TBUS2_NS_BEGIN

////////////////////////////////////////////////////////////////////////////////

class EndpointWrap {
 public:
  using Callback = std::function<int(tbuspp_endpoint_t *, const tbuspp_event_t *evt)>;

 private:
  typedef std::vector<std::pair<int, Callback>> CallbackVec;
  tbuspp_endpoint_t *ep_ = nullptr;
  tbuspp_queue_t *inq_ = nullptr;
  tbuspp_queue_t *outq_ = nullptr;
  CallbackVec cbs_;

 public:
  EndpointWrap() = default;
  explicit EndpointWrap(tbuspp_endpoint_t* ep) {
    Attach(ep);
  }
  explicit EndpointWrap(const EndpointWrap&) = delete;
  EndpointWrap& operator=(const EndpointWrap&) = delete;

  EndpointWrap(EndpointWrap&& other) {
    Attach(other.Detach());
  }

  virtual ~EndpointWrap() { Close(); }

  bool Attach(tbuspp_endpoint_t *ep) {
    if (ep_ != nullptr || ep == nullptr) {
      return false;
    }

    ep_ = ep;
    tbuspp_set_callback(ep_, EndpointWrap::CallbackEntry, this);
    SetupMqs();
    return true;
  }

  tbuspp_endpoint_t* Detach() {
    if (ep_ == nullptr) {
      return nullptr;
    }

    auto ep = ep_;
    ep_ = nullptr;
    inq_ = nullptr;
    outq_ = nullptr;

    tbuspp_set_callback(ep, nullptr, nullptr);
    return ep;
  }

  // make command channel with tbus2_agent, and register endpoint to namesvr(tbus2_ns)
  int Open(const tbuspp_endpoint_conf_t &conf, int wait_ms, const tbuspp_context_t* ctx) {
    if (ep_ != nullptr) {
      return TBUSPP_ERR_OP_DENIED;
    }

    int err = TBUSPP_ERR_OK;
    auto ep = tbuspp_open(&conf, wait_ms, ctx, &err);
    if (err != TBUSPP_ERR_OK) {
      return err;
    }

    tbuspp_set_callback(ep, EndpointWrap::CallbackEntry, this);
    ep_ = ep;
    if (wait_ms > 0) {
      SetupMqs();
    }

    return TBUSPP_ERR_OK;
  }

  // unregister endpoint, and close command channel
  void Close() {
    auto ep = ep_;
    if (ep != nullptr) {
      ep_ = nullptr;
      inq_ = nullptr;
      outq_ = nullptr;
      tbuspp_close(ep);
    }
  }

  // Open = Connect + Registr
  // When complete Connect() call, could send some RPC request to namesvr(tbus2_ns)
  int Connect(const std::string& agent_url, int wait_ms, const tbuspp_context_t* ctx) {
    if (ep_ != nullptr) {
      return TBUSPP_ERR_OP_DENIED;
    }

    int err = TBUSPP_ERR_OK;
    auto ep = tbuspp_connect(agent_url.c_str(), wait_ms, ctx, &err);
    if (err != TBUSPP_ERR_OK) {
      return err;
    }

    tbuspp_set_callback(ep, EndpointWrap::CallbackEntry, this);
    ep_ = ep;
    return TBUSPP_ERR_OK;
  }

  int Register(const tbuspp_endpoint_conf_t& conf, int wait_ms,
               const tbuspp_context_t* ctx) {
    if (ep_ == nullptr) {
      return TBUSPP_ERR_OP_DENIED;
    }
    return tbuspp_register(ep_, &conf, wait_ms, ctx);
  }

  // Shoud call Udpate() intervally
  // keep internal communication with tbus2_agent
  // Callback will be invoked in Update() call
  int Update(int wait_ms) {
    int ret = tbuspp_update(ep_, wait_ms);
    if (inq_ == nullptr && IsConnected()) {
      // complete async open
      SetupMqs();
    }

    return ret;
  }

  // improve callback support, enable callback chain
  bool AddCallback(int fun_id, Callback cb) {
    for (auto [fid, _] : cbs_) {
      if (fid == fun_id) {
        return false;
      }
    }

    cbs_.push_back({fun_id, cb});
    return true;
  }

  void DelCallback(int fun_id) {
    for (auto it = cbs_.begin(); it != cbs_.end(); ++it) {
      if (it->first == fun_id) {
        cbs_.erase(it);
        return;
      }
    }
  }

  // enable recv P2G msg
  int JoinGroup(int wait_ms, const tbuspp_context_t* ctx) {
    return tbuspp_join_group(ep_, wait_ms, ctx);
  }

  int ExitGroup(int wait_ms, const tbuspp_context_t* ctx) {
    return tbuspp_exit_group(ep_, wait_ms, ctx);
  }

  tbuspp_endpoint_t* GetEndpoint()const { return ep_; }
  tbuspp_queue_t* GetInputQueue()const { return inq_; }
  tbuspp_queue_t* GetOutputQueue()const { return outq_; }

  tbuspp_id_t GetBusId()const { return tbuspp_get_busid(ep_); }
  bool IsConnected()const { return tbuspp_is_connected(ep_); }
  bool IsReady()const { return tbuspp_is_ready(ep_); }
  int GetCmdConnFd()const { return tbuspp_endpoint_fd(ep_); }

  uint32_t GetDomainId()const { return tbuspp_get_domain_id(ep_); }
  const char* GetDomainAlias()const { return tbuspp_get_domain_alias(ep_); }
  tbuspp_id_t GetAgentId()const { return tbuspp_get_agent_id(ep_); }
  uint32_t GetAgentVersion()const { return tbuspp_get_agent_version(ep_); }

  // return <retcode, status>
  std::pair<int, int> QueryEndpointStatus(tbuspp_id_t busid, int wait_ms,
                                          const tbuspp_context_t* ctx) {
    int status = TBUSPP_ENDPOINT_STATUS_UNK;
    int ret = tbuspp_query_endpoint_status(ep_, busid, wait_ms, ctx, &status);
    return {ret, status};
  }

  // return <retcode, udata_string>
  std::pair<int, std::string> QueryEndpointUData(tbuspp_id_t busid, int wait_ms,
                                                 const tbuspp_context_t* ctx) {
    char buf[TBUSPP_ENDPOINT_UDATA_MAX_SIZE];
    size_t val_size = 0;
    int err = tbuspp_get_endpoint_udata(ep_, busid, buf, sizeof(buf), &val_size, wait_ms, ctx);\
    if (err != TBUSPP_ERR_OK) {
      return {err, ""};
    }
    return {TBUSPP_ERR_OK, std::string{buf, val_size}};
  }

  // Cache Group/Endpoint data in Api, more cache Api see tbuspp_cache_* functions
  // return <retcode, udata_string>
  std::pair<int, std::string_view> CacheGetEndpointUData(tbuspp_id_t busid) const {
    const char* buf = nullptr;
    size_t size = 0;
    int err = tbuspp_cache_get_endpoint_udata(ep_, busid, &buf, &size);
    if (err != TBUSPP_ERR_OK) {
      return {err, ""};
    }
    return {TBUSPP_ERR_OK, std::string_view(buf, size)};
  }

  // some queue operation wraps
  // more mq operations see tbuspp_queue_* functions
  int WriteMsg(tbuspp_id_t dest, const void *msg_data, size_t msg_size,
    const tbuspp_msg_param_t* param) {
    if (outq_ == nullptr) {
      return TBUSPP_ERR_OP_DENIED;
    }
    return tbuspp_queue_write(outq_, dest, msg_data, msg_size, param);
  }

  int WriteMsgV(tbuspp_id_t dest, const iovec *iov, int iov_num, const tbuspp_msg_param_t *param) {
    if (outq_ == nullptr) {
      return TBUSPP_ERR_OP_DENIED;
    }

    return tbuspp_queue_writev(outq_, dest, iov, iov_num, param);
  }

  // using LockBuf & CommitBuf to support ZeroCopy transfer msg
  // return  <retcode, msg_buffer_ptr>
  std::pair<int, char*> LockBuf(tbuspp_id_t dest, const tbuspp_msg_param_t* param,
    uint32_t max_msg_size) {
    if (outq_ == nullptr) {
      return {TBUSPP_ERR_OP_DENIED, nullptr};
    }

    char *msgbuf = nullptr;
    int ret = tbuspp_queue_lock_buf(outq_, dest, param, max_msg_size, &msgbuf);
    return {ret, msgbuf};
  }

  int CommitBuf(uint32_t msg_size) {
    if (outq_ == nullptr) {
      return TBUSPP_ERR_OP_DENIED;
    }

    return tbuspp_queue_commit_buf(outq_, msg_size);
  }

  // \desc save msg description info, optional
  // \return <msgbuf, msgsize>
  std::pair<const char*, uint32_t> PeekMsg(tbuspp_msg_desc_ex_t* desc) {
    if (inq_ == nullptr) {
      return {nullptr, 0};
    }

    if (desc != nullptr) {
      tbuspp_init_msg_desc_ex(desc);
    }

    uint32_t msg_size = 0;
    const char *msg_data = tbuspp_queue_peek_desc_ex(inq_, &msg_size, desc);
    return {msg_data, msg_size};
  }

  // \cursor  peek the first msg after cursor, if null, then return the first msg in input mq
  // \return  <retcode, msg>
  // use `tbuspp_msg_get_data_ex` to extract msg info from msg object
  // if msg processed but should be kept in input mq, then call `tbuspp_msg_mark_discard` to
  // discard it, it is useful for graceful-restart gamesvr
  std::pair<int, tbuspp_msg_t*> PeekMsgEx(tbuspp_msg_t *cursor) {
    if (inq_ == nullptr) {
      return {TBUSPP_ERR_OP_DENIED, nullptr};
    }

    int ret = 0;
    auto msg = tbuspp_queue_peek_ex(inq_, cursor, &ret);
    return {ret, msg};
  }

  void PopMsg() {
    if (inq_ == nullptr) {
      return;
    }

    tbuspp_queue_pop(inq_);
  }

  int ClearInputQueue() {
    if (inq_ == nullptr) {
      return 0;
    }

    return tbuspp_queue_clear(inq_);
  }

  void SetupMqs() {
    assert(ep_ != nullptr);
    inq_ = tbuspp_get_input_queue(ep_);
    outq_ = tbuspp_get_output_queue(ep_);
  }

 private:
  static int CallbackEntry(tbuspp_endpoint_t *ep, const tbuspp_event_t *evt, void *udata) {
    EndpointWrap *wrap = reinterpret_cast<EndpointWrap*>(udata);
    assert(wrap != nullptr);
    return wrap->HandleCallback(ep, evt);
  }

  int HandleCallback(tbuspp_endpoint_t *ep, const tbuspp_event_t *evt) {
    for (const auto &[_, cb] : cbs_) {
      int ret = cb(ep, evt);
      if (ret < 0) {
        return ret;
      }
    }

    return 0;
  }
};

////////////////////////////////////////////////////////////////////////////////

TBUS2_NS_END
#endif  // TBUSPP2_INC_CPP_ENDPOINT_HPP_
