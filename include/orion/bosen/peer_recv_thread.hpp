#pragma once

#include <memory>
#include <vector>

#include <orion/noncopyable.hpp>
#include <orion/bosen/conn.hpp>
#include <orion/bosen/message.hpp>
#include <orion/bosen/execute_message.hpp>
#include <orion/bosen/conn.hpp>
#include <orion/bosen/event_handler.hpp>
#include <orion/bosen/byte_buffer.hpp>
#include <orion/bosen/peer_recv_buffer.hpp>
#include <orion/bosen/recv_arbitrary_bytes.hpp>

namespace orion {
namespace bosen {

class PeerRecvThread {
  struct PollConn {
    enum class ConnType {
      peer = 1,
        executor = 2,
        server = 3
    };
    void* conn;
    ConnType type;
    int32_t id;

    bool Receive() {
      if (type == ConnType::peer
          || type == ConnType::server) {
        auto* sock_conn = reinterpret_cast<conn::SocketConn*>(conn);
        return sock_conn->sock.Recv(&(sock_conn->recv_buff));
      } else {
        auto* pipe_conn = reinterpret_cast<conn::PipeConn*>(conn);
        return pipe_conn->pipe.Recv(&(pipe_conn->recv_buff));
      }
    }

    bool Send() {
      if (type == ConnType::peer
          || type == ConnType::server) {
        auto* sock_conn = reinterpret_cast<conn::SocketConn*>(conn);
        return sock_conn->sock.Send(&(sock_conn->send_buff));
      } else {
        auto* pipe_conn = reinterpret_cast<conn::PipeConn*>(conn);
        return pipe_conn->pipe.Send(&(pipe_conn->send_buff));
      }
    }

    conn::RecvBuffer& get_recv_buff() {
      if (type == ConnType::peer
          || type == ConnType::server) {
        return reinterpret_cast<conn::SocketConn*>(conn)->recv_buff;
      } else {
        return reinterpret_cast<conn::PipeConn*>(conn)->recv_buff;
      }
    }

    conn::SendBuffer& get_send_buff() {
      if (type == ConnType::peer
          || type == ConnType::server) {
        return reinterpret_cast<conn::SocketConn*>(conn)->send_buff;
      } else {
        return reinterpret_cast<conn::PipeConn*>(conn)->send_buff;
      }
    }

    bool is_connect_event() const {
      return false;
    }

    int get_read_fd() const {
      if (type == ConnType::peer
          || type == ConnType::server) {
        auto* sock_conn = reinterpret_cast<conn::SocketConn*>(conn);
        return sock_conn->sock.get_fd();
      } else {
        auto* pipe_conn = reinterpret_cast<conn::PipeConn*>(conn);
        return pipe_conn->pipe.get_read_fd();
      }
    }

    int get_write_fd() const {
      if (type == ConnType::peer
          || type == ConnType::server) {
        auto* sock_conn = reinterpret_cast<conn::SocketConn*>(conn);
        return sock_conn->sock.get_fd();
      } else {
        auto* pipe_conn = reinterpret_cast<conn::PipeConn*>(conn);
        return pipe_conn->pipe.get_write_fd();
      }
    }
  };

  enum class Action {
    kNone = 0,
      kExit = 1,
      kAckConnectToPeers = 2
            };

  const int32_t kId;
  const int32_t kExecutorId;
  const int32_t kServerId;
  const bool kIsServer;
  const size_t kNumExecutors;
  const size_t kNumServers;
  const size_t kCommBuffCapacity;

  EventHandler<PollConn> event_handler_;
  Blob send_mem_;
  conn::SendBuffer send_buff_;

  Blob peer_send_mem_;
  Blob peer_recv_mem_;
  std::vector<std::unique_ptr<conn::SocketConn>> peer_;
  std::vector<PollConn> peer_conn_;
  std::vector<conn::Socket> peer_socks_;
  std::vector<int> peer_sock_fds_;
  std::vector<ByteBuffer> peer_recv_byte_buff_;

  Blob server_send_mem_;
  Blob server_recv_mem_;
  std::vector<std::unique_ptr<conn::SocketConn>> server_;
  std::vector<PollConn> server_conn_;
  std::vector<conn::Socket> server_socks_;
  std::vector<int> server_sock_fds_;
  std::vector<ByteBuffer> server_recv_byte_buff_;

  conn::Pipe executor_pipe_[2];
  Blob executor_recv_mem_;
  Blob executor_send_mem_;
  std::unique_ptr<conn::PipeConn> executor_;
  PollConn executor_conn_;
  size_t num_identified_peers_ {0};
  Action action_ { Action::kNone };

  void *data_recv_buff_ { nullptr };
  std::unique_ptr<PeerRecvExecForLoopDistArrayDataBuffer> exec_for_loop_data_buff_;
 public:
  PeerRecvThread(int32_t id,
                 int32_t executor_id,
                 int32_t server_id,
                 bool is_server,
                 const std::vector<conn::Socket> &peer_socks,
                 const std::vector<conn::Socket> &server_socks,
                 size_t buff_capacity);
  void operator() ();
  conn::Pipe GetExecutorPipe();
 private:
  int HandleMsg(PollConn* poll_conn_ptr);
  int HandlePeerMsg(PollConn* poll_conn_ptr);
  int HandleExecuteMsg(PollConn* poll_conn_ptr);
  int HandleExecutorMsg();
  int HandleClosedConnection(PollConn *poll_conn_ptr);
  void ExecForLoopServeRequest();
  void SendToExecutor();
};

PeerRecvThread::PeerRecvThread(
    int32_t id,
    int32_t executor_id,
    int32_t server_id,
    bool is_server,
    const std::vector<conn::Socket> &peer_socks,
    const std::vector<conn::Socket> &server_socks,
    size_t buff_capacity):
    kId(id),
    kExecutorId(executor_id),
    kServerId(server_id),
    kIsServer(is_server),
    kNumExecutors(peer_socks.size()),
    kNumServers(server_socks.size()),
    kCommBuffCapacity(buff_capacity),
    send_mem_(kCommBuffCapacity),
    send_buff_(send_mem_.data(), kCommBuffCapacity),
    peer_send_mem_(buff_capacity * kNumExecutors),
    peer_recv_mem_(buff_capacity * kNumExecutors),
    peer_(kNumExecutors),
    peer_conn_(kNumExecutors),
    peer_socks_(peer_socks),
    peer_sock_fds_(kNumExecutors),
    peer_recv_byte_buff_(kNumExecutors),
    server_send_mem_(buff_capacity * kNumServers),
    server_recv_mem_(buff_capacity * kNumServers),
    server_(kNumServers),
    server_conn_(kNumServers),
    server_socks_(server_socks),
    server_sock_fds_(kNumServers),
    server_recv_byte_buff_(kNumServers),
    executor_recv_mem_(buff_capacity),
    executor_send_mem_(buff_capacity),
    num_identified_peers_(0) {
  int ret = conn::Pipe::CreateBiPipe(executor_pipe_);
  CHECK_EQ(ret, 0) << "create pipe failed";

  executor_ = std::make_unique<conn::PipeConn>(
      executor_pipe_[0],
      executor_recv_mem_.data(),
      executor_send_mem_.data(),
      kCommBuffCapacity);
  executor_conn_.type = PollConn::ConnType::executor;
  executor_conn_.conn = executor_.get();
}

void
PeerRecvThread::operator() () {

  event_handler_.SetClosedConnectionHandler(
      std::bind(&PeerRecvThread::HandleClosedConnection, this,
                std::placeholders::_1));

  event_handler_.SetReadEventHandler(
      std::bind(&PeerRecvThread::HandleMsg, this, std::placeholders::_1));

  event_handler_.SetDefaultWriteEventHandler();

  event_handler_.SetToReadOnly(&executor_conn_);

  for (size_t peer_index = 0; peer_index < kNumExecutors; peer_index++) {
    if (!kIsServer && peer_index == kExecutorId) continue;
    auto &sock = peer_socks_[peer_index];
    uint8_t *recv_mem = peer_recv_mem_.data()
                        + kCommBuffCapacity * peer_index;

    uint8_t *send_mem = peer_send_mem_.data()
                        + kCommBuffCapacity * peer_index;

    auto *sock_conn = new conn::SocketConn(
        sock, recv_mem, send_mem, kCommBuffCapacity);
    auto &curr_poll_conn = peer_conn_[peer_index];
    curr_poll_conn.conn = sock_conn;
    curr_poll_conn.type = PollConn::ConnType::peer;
    curr_poll_conn.id = peer_index;
    int ret = event_handler_.SetToReadOnly(&curr_poll_conn);
    CHECK_EQ(ret, 0) << "errno = " << errno << " fd = " << sock.get_fd()
                     << " i = " << peer_index
                     << " id = " << kId;
  }

  if (!kIsServer) {
    for (size_t server_index = 0; server_index < kNumServers; server_index++) {
      auto &sock = server_socks_[server_index];
      uint8_t *recv_mem = server_recv_mem_.data()
                          + kCommBuffCapacity * server_index;

      uint8_t *send_mem = server_send_mem_.data()
                          + kCommBuffCapacity * server_index;

      auto *sock_conn = new conn::SocketConn(
          sock, recv_mem, send_mem, kCommBuffCapacity);
      auto &curr_poll_conn = server_conn_[server_index];
      curr_poll_conn.conn = sock_conn;
      curr_poll_conn.type = PollConn::ConnType::server;
      curr_poll_conn.id = server_index;
      int ret = event_handler_.SetToReadOnly(&curr_poll_conn);
      CHECK_EQ(ret, 0) << "errno = " << errno << " fd = " << sock.get_fd()
                       << " i = " << server_index
                       << " id = " << kId;
    }
  }

  while (true) {
    event_handler_.WaitAndHandleEvent();
    if (action_ == Action::kExit) break;
  }
}

conn::Pipe
PeerRecvThread::GetExecutorPipe() {
  return executor_pipe_[1];
}

int
PeerRecvThread::HandleMsg(PollConn* poll_conn_ptr) {
  int ret = 0;
  if (poll_conn_ptr->type == PollConn::ConnType::peer) {
    ret = HandlePeerMsg(poll_conn_ptr);
  } else {
    ret = HandleExecutorMsg();
  }

  while (action_ != Action::kNone
         && action_ != Action::kExit) {
    switch (action_) {
      case Action::kExit:
        break;
      case Action::kAckConnectToPeers:
        {
          message::Helper::CreateMsg<message::ExecutorConnectToPeersAck>(&send_buff_);
          send_buff_.set_next_to_send(
              peer_sock_fds_.data(), peer_sock_fds_.size() * sizeof(int));
          SendToExecutor();
          send_buff_.clear_to_send();
          action_ = Action::kNone;
        }
        break;
      default:
        LOG(FATAL) << "unknown";
    }
  }
  return  ret;
}

int
PeerRecvThread::HandleExecutorMsg() {
  auto &recv_buff = executor_->recv_buff;

  auto msg_type = message::Helper::get_type(recv_buff);
  CHECK(msg_type == message::Type::kExecuteMsg)
      << " type = " << static_cast<int>(msg_type);
  auto exec_msg_type = message::ExecuteMsgHelper::get_type(recv_buff);
  int ret = EventHandler<PollConn>::kNoAction;
  switch (exec_msg_type) {
    case message::ExecuteMsgType::kPeerRecvStop:
      {
        action_ = Action::kExit;
        ret = EventHandler<PollConn>::kClearOneMsg | EventHandler<PollConn>::kExit;
      }
      break;
    case message::ExecuteMsgType::kRequestExecForLoopDistArrayData:
      {
        if (exec_for_loop_data_buff_.get() == nullptr) {
          exec_for_loop_data_buff_.reset(new PeerRecvExecForLoopDistArrayDataBuffer());
        }
        exec_for_loop_data_buff_->is_executor_expecting = true;
        ExecForLoopServeRequest();
        ret = EventHandler<PollConn>::kClearOneMsg;
      }
      break;
    default:
      LOG(FATAL) << "unknown exec msg " << static_cast<int>(exec_msg_type);
  }
  return ret;
}

int
PeerRecvThread::HandlePeerMsg(PollConn* poll_conn_ptr) {
  auto &recv_buff = poll_conn_ptr->get_recv_buff();

  auto msg_type = message::Helper::get_type(recv_buff);
  int ret = EventHandler<PollConn>::kClearOneMsg;
  switch (msg_type) {
    case message::Type::kExecutorIdentity:
      {
        auto *msg = message::Helper::get_msg<message::ExecutorIdentity>(recv_buff);
        auto* sock_conn = reinterpret_cast<conn::SocketConn*>(poll_conn_ptr->conn);
        peer_[msg->executor_id].reset(sock_conn);
        peer_sock_fds_[msg->executor_id] = sock_conn->sock.get_fd();
        poll_conn_ptr->id = msg->executor_id;
        num_identified_peers_++;
        if (kIsServer) {
          if (num_identified_peers_ == kNumExecutors) {
            action_ = Action::kAckConnectToPeers;
          } else {
            action_ = Action::kNone;
          }
        } else {
          if (num_identified_peers_ == kExecutorId) {
            action_ = Action::kAckConnectToPeers;
          } else {
            action_ = Action::kNone;
          }
        }
        ret = EventHandler<PollConn>::kClearOneMsg;
      }
      break;
    case message::Type::kExecuteMsg:
      {
        ret = HandleExecuteMsg(poll_conn_ptr);
      }
      break;
    default:
      {
        LOG(FATAL) << "unknown message type " << static_cast<int>(msg_type)
                   << " from " << poll_conn_ptr->id;
      }
      break;
  }
  return ret;
}

int
PeerRecvThread::HandleExecuteMsg(PollConn* poll_conn_ptr) {
  auto &recv_buff = poll_conn_ptr->get_recv_buff();
  auto &sock = *reinterpret_cast<conn::Socket*>(poll_conn_ptr->conn);
  auto msg_type = message::ExecuteMsgHelper::get_type(recv_buff);
  int ret = EventHandler<PollConn>::kClearOneMsg;
  int32_t sender_id = poll_conn_ptr->id;
  switch (msg_type) {
    case message::ExecuteMsgType::kRepartitionDistArrayData:
      {
        auto *msg = message::ExecuteMsgHelper::get_msg<message::ExecuteMsgRepartitionDistArrayData>(
            recv_buff);
        size_t expected_size = msg->data_size;
        bool received_next_msg = (expected_size == 0);
        if (data_recv_buff_ == nullptr) {
          auto *buff_ptr = new PeerRecvRepartitionDistArrayDataBuffer();
          buff_ptr->dist_array_id = msg->dist_array_id;
          data_recv_buff_ = buff_ptr;
        }
        auto *repartition_recv_buff =
            reinterpret_cast<PeerRecvRepartitionDistArrayDataBuffer*>(
                data_recv_buff_);
        if (expected_size > 0) {
          auto &byte_buffs = repartition_recv_buff->byte_buffs;
          auto &byte_buff = byte_buffs[sender_id];
          if (byte_buff.GetCapacity() == 0) byte_buff.Reset(expected_size);
          received_next_msg =
              ReceiveArbitraryBytes(
                  sock, &recv_buff,
                  &byte_buff, expected_size);
        }

        if (received_next_msg) {
          ret = expected_size > 0
                ? EventHandler<PollConn>::kClearOneAndNextMsg
                : EventHandler<PollConn>::kClearOneMsg;
          repartition_recv_buff->num_executors_received += 1;
          if (repartition_recv_buff->num_executors_received
              == (kNumExecutors - 1)) {
            message::ExecuteMsgHelper::CreateMsg<
              message::ExecuteMsgRepartitionDistArrayRecved>(
                &send_buff_, data_recv_buff_);
            data_recv_buff_ = nullptr;
            SendToExecutor();
            send_buff_.clear_to_send();
          }
        } else {
          ret = EventHandler<PollConn>::kNoAction;
        }
        action_ = Action::kNone;
      }
      break;
    case message::ExecuteMsgType::kPipelineTimePartition:
      {
        auto* msg = message::ExecuteMsgHelper::get_msg<message::ExecuteMsgPipelineTimePartition>(
            recv_buff);
        size_t expected_size = msg->data_size;

        if (exec_for_loop_data_buff_.get() == nullptr) {
          exec_for_loop_data_buff_.reset(new PeerRecvExecForLoopDistArrayDataBuffer());
        }

        auto &incomplete_buffers = exec_for_loop_data_buff_->incomplete_buffers;
        auto iter = incomplete_buffers.find(sender_id);
        if (iter == incomplete_buffers.end()) {
          auto iter_pair = incomplete_buffers.emplace(std::make_pair(
              sender_id, PeerRecvDistArrayDataBuffer()));
          iter = iter_pair.first;
          iter->second.dist_array_id = msg->dist_array_id;
          iter->second.partition_id = msg->time_partition_id;
          iter->second.data = new uint8_t[expected_size];
          iter->second.expected_size = expected_size;
          iter->second.received_size = 0;
        }

        bool received_next_msg = ReceiveArbitraryBytes(sock, &recv_buff,
                                                       iter->second.data, &(iter->second.received_size),
                                                       expected_size);
        if (received_next_msg) {
          ret = EventHandler<PollConn>::kClearOneAndNextMsg;
          action_ = Action::kNone;
          exec_for_loop_data_buff_->complete_buffers.emplace_back(iter->second);
          incomplete_buffers.erase(iter);
          ExecForLoopServeRequest();
        } else {
          ret = EventHandler<PollConn>::kNoAction;
          action_ = Action::kNone;
        }
      }
      break;
    default:
      LOG(FATAL) << "unexpected message type = " << static_cast<int>(msg_type);
  }
  return ret;
}

int
PeerRecvThread::HandleClosedConnection(PollConn *poll_conn_ptr) {
  int ret = event_handler_.Remove(poll_conn_ptr);
  CHECK_EQ(ret, 0);

  return EventHandler<PollConn>::kNoAction;
}

void
PeerRecvThread::ExecForLoopServeRequest() {
  if (!exec_for_loop_data_buff_->is_executor_expecting) return;
  if (exec_for_loop_data_buff_->complete_buffers.empty()) return;
  auto& complete_buffers = exec_for_loop_data_buff_->complete_buffers;
  size_t data_buff_size = complete_buffers.size() * sizeof(PeerRecvDistArrayDataBuffer);
  uint8_t *data_buff_vec = new uint8_t[data_buff_size];
  memcpy(data_buff_vec, complete_buffers.data(), data_buff_size);

  message::ExecuteMsgHelper::CreateMsg<message::ExecuteMsgReplyExecForLoopDistArrayData>(
      &send_buff_, data_buff_vec, complete_buffers.size());
  complete_buffers.clear();
  SendToExecutor();
  send_buff_.clear_to_send();
  send_buff_.reset_sent_sizes();

  if (exec_for_loop_data_buff_->incomplete_buffers.empty()) {
    exec_for_loop_data_buff_.reset();
  } else {
    exec_for_loop_data_buff_->is_executor_expecting = false;
  }
}

void
PeerRecvThread::SendToExecutor() {
  auto& send_buff = executor_conn_.get_send_buff();
  if (send_buff.get_remaining_to_send_size() > 0
      || send_buff.get_remaining_next_to_send_size() > 0) {
    bool sent = executor_->pipe.Send(&send_buff);
    while (!sent) {
      sent = executor_->pipe.Send(&send_buff);
    }
    send_buff.clear_to_send();
  }
  bool sent = executor_->pipe.Send(&send_buff_);
  if (!sent) {
    send_buff.CopyAndMoveNextToSend(&send_buff_);
    event_handler_.SetToReadWrite(&executor_conn_);
  }
  send_buff_.reset_sent_sizes();
}

}
}