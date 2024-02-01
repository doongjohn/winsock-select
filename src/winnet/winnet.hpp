#pragma once

#include <mutex>
#include <span>
#include <queue>
#include <vector>
#include <string>
#include <functional>
#include <unordered_map>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <mswsock.h>

namespace winnet {

auto wsa_init() -> bool;

auto wsa_deinit() -> bool;

#pragma pack(push, 1)
struct PacketHeader {
  uint32_t packet_size;
};
#pragma pack(pop)

struct SendQueue {
private:
  std::mutex mutex;
  std::queue<std::vector<char>> queue;

public:
  SendQueue();
  SendQueue(SendQueue &old);
  SendQueue(SendQueue &&old) noexcept;

  auto is_empty() -> bool;

  auto push_back(const std::vector<char> &data) -> void;
  auto pop_front() -> std::vector<char>;
};

struct Connection {
  friend struct ConnectionHandler;

public:
  SOCKET socket;
  sockaddr_in addr_info;
  std::string ip;
  std::string username;

private:
  std::vector<char> recv_buf;
  uint32_t recv_total_size;
  uint32_t cur_recv_amount;
  bool is_recv_header;

  SendQueue send_queue;
  std::vector<char> send_buf;
  uint32_t cur_send_amount;

public:
  Connection();
  Connection(SOCKET socket, SOCKADDR_IN addr_info);

  auto close() -> void;
  auto send(const std::span<const char> data) -> void;

  auto get_recv_string() -> std::string;
  auto get_recv_bytes() -> std::vector<char>;
};

struct ConnectionHandler;

template <typename T>
struct ConnectionCallbacks {
  std::function<void(T *, ConnectionHandler &, int)> on_select_error;
  std::function<void(T *, ConnectionHandler &)> on_select_timeout;
  std::function<void(T *, int)> on_conn_accept_error;
  std::function<void(T *, Connection &)> on_conn_connected;
  std::function<void(T *, Connection &)> on_conn_ended;
  std::function<void(T *, Connection &, int)> on_recv_error;
  std::function<void(T *, Connection &)> on_recv_success;
  std::function<void(T *, Connection &, int)> on_send_error;
  std::function<void(T *, Connection &)> on_send_success;

  ConnectionCallbacks() {
    // clang-format off
    on_select_error = [](T *, ConnectionHandler &, int) {};
    on_select_timeout = [](T *, ConnectionHandler &) {};
    on_conn_accept_error = [](T *, int) {};
    on_conn_connected = [](T *, Connection &) {};
    on_conn_ended = [](T *, Connection &) {};
    on_recv_error = [](T *, Connection &, int) {};
    on_recv_success = [](T *, Connection &) {};
    on_send_error = [](T *, Connection &, int) {};
    on_send_success = [](T *, Connection &) {};
    // clang-format on
  };
};

class NetEntity {
public:
  inline static u_long BLOCKING = 0ul;
  inline static u_long NONBLOCKING = 1ul;

  ConnectionCallbacks<NetEntity> base_callbacks;

  NetEntity();
  virtual ~NetEntity();

  std::unordered_map<SOCKET, Connection> connections;

  auto send_all(const std::span<const char> data) -> void;
  auto send_to(const std::span<SOCKET> targets, const std::span<const char> data) -> void;
  auto send_all_but(const std::span<SOCKET> ignore_targets, const std::span<const char> data) -> void;
};

class Server final : public NetEntity {
public:
  SOCKET listen_socket;
  uint16_t port;

  ConnectionCallbacks<Server> callbacks;

  Server();
  ~Server() override;

  auto init() -> bool;
  auto bind(uint16_t port) -> bool;
  auto listen() -> bool;
};

class Client final : public NetEntity {
public:
  Connection *connection;

  ConnectionCallbacks<Client> callbacks;

  Client();
  ~Client() override;

  auto init() -> bool;
  auto connect(ConnectionHandler &connection_handler, std::string ip, std::string port) -> bool;
  auto disconnect(ConnectionHandler &connection_handler) -> void;
};

struct ConnectionHandler {
  NetEntity *net_entity;
  ConnectionCallbacks<NetEntity> &callbacks;

  fd_set read_set;
  fd_set write_set;
  fd_set err_set;

  ConnectionHandler(NetEntity *net_entity);

  auto init() -> void;
  auto is_full() -> bool;
  auto tick(timeval timeout) -> bool;
  auto run(timeval timeout, bool &stop_flag) -> bool;
  auto run(timeval timeout, std::atomic_bool &stop_flag) -> bool;
};

} // namespace winnet
