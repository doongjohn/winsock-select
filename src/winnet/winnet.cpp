#define WIN32_LEAN_AND_MEAN

#include "winnet.hpp"

#include <array>
#include <vector>
#include <queue>
#include <format>
#include <iostream>

#include <utils.hpp>

namespace winnet {

auto wsa_init() -> bool {
  // initialize winsock
  auto wsa_data = WSADATA{};
  auto startup_result = ::WSAStartup(MAKEWORD(2, 2), &wsa_data);
  if (startup_result != 0) {
    std::cerr << std::format("[winsock error] WSAStartup failed (error code: {})\n", startup_result);
    return false;
  }

  return true;
}

auto wsa_deinit() -> bool {
  if (::WSACleanup() == SOCKET_ERROR) {
    utils::print_wsa_error("[winsock error] WSACleanup failed");
    return false;
  }

  return true;
}

SendQueue::SendQueue() : mutex{}, queue{} {}

SendQueue::SendQueue(SendQueue &old) : mutex{}, queue{old.queue} {}

SendQueue::SendQueue(SendQueue &&old) noexcept : mutex{}, queue{std::move(old.queue)} {}

auto SendQueue::is_empty() -> bool {
  const auto lock = std::scoped_lock{mutex};
  return queue.empty();
}

auto SendQueue::push_back(const std::vector<char> &data) -> void {
  const auto lock = std::scoped_lock{mutex};
  queue.emplace(data);
}

auto SendQueue::pop_front() -> std::vector<char> {
  const auto lock = std::scoped_lock{mutex};
  auto data = queue.front();
  queue.pop();
  return data;
}

Connection::Connection()
    : socket{INVALID_SOCKET}, addr_info{}, recv_buf{}, recv_total_size{0}, cur_recv_amount{0}, is_recv_header(true),
      send_queue{}, send_buf{}, cur_send_amount{0} {}

Connection::Connection(SOCKET socket, SOCKADDR_IN addr_info)
    : socket{socket}, addr_info{addr_info}, recv_buf{}, recv_total_size{0}, cur_recv_amount{0}, is_recv_header(true),
      send_queue{}, send_buf{}, cur_send_amount{0} {
  ip = std::string(INET_ADDRSTRLEN, '\0');
  ::inet_ntop(AF_INET, &addr_info.sin_addr, ip.data(), ip.length());
}

auto Connection::close() -> void {
  if (::closesocket(this->socket) == SOCKET_ERROR) {
    utils::print_wsa_error("[winsock error] closesocket failed");
  }
}

auto Connection::send(const std::span<const char> data) -> void {
  if (data.empty()) {
    return;
  }

  const auto header = PacketHeader{
    .packet_size = static_cast<uint32_t>(data.size()),
  };
  const auto header_size = sizeof(header);

  auto data_alloced = std::vector<char>{};
  data_alloced.resize(header_size + data.size());

  std::memcpy(data_alloced.data(), &header, header_size);
  std::memcpy(data_alloced.data() + header_size, data.data(), data.size());

  // add to send queue
  send_queue.push_back(data_alloced);
}

auto Connection::get_recv_string() -> std::string {
  return std::string{recv_buf.begin(), recv_buf.end()};
}

auto Connection::get_recv_bytes() -> std::vector<char> {
  return recv_buf;
}

NetEntity::NetEntity() = default;

NetEntity::~NetEntity() {
  for (auto &[sock, conn] : connections) {
    ::closesocket(sock);
  }
}

auto NetEntity::send_all(const std::span<const char> data) -> void {
  for (auto &[sock, conn] : connections) {
    conn.send(data);
  }
}

auto NetEntity::send_to(const std::span<SOCKET> targets, const std::span<const char> data) -> void {
  for (auto &sock : targets) {
    connections.at(sock).send(data);
  }
}

auto NetEntity::send_all_but(const std::span<SOCKET> ignore_targets, const std::span<const char> data) -> void {
  for (auto &[sock, conn] : connections) {
    if (std::find(ignore_targets.begin(), ignore_targets.end(), sock) == ignore_targets.end()) {
      conn.send(data);
    }
  }
}

Server::Server() : listen_socket{INVALID_SOCKET}, port{0} {}

Server::~Server() {
  ::closesocket(listen_socket);
}

auto Server::init(uint16_t port) -> bool {
  // create a socket for the server to listen for client connections
  listen_socket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
  if (listen_socket == INVALID_SOCKET) {
    utils::print_wsa_error("[winsock error] socket creation failed");
    return false;
  }

  // bind
  this->port = port;
  auto addr_hint = sockaddr_in{};
  addr_hint.sin_family = AF_INET;
  addr_hint.sin_addr.S_un.S_addr = ::htonl(INADDR_ANY); // INADDR_ANY == 0.0.0.0
  addr_hint.sin_port = ::htons(port);                   // htonl, htons -> little endian에서 big endian으로 변환

  if (::bind(listen_socket, std::bit_cast<sockaddr *>(&addr_hint), sizeof(addr_hint)) == SOCKET_ERROR) {
    utils::print_wsa_error("[winsock error] bind failed");
    return false;
  }

  return true;
}

auto Server::listen() -> bool {
  if (::listen(listen_socket, SOMAXCONN) == SOCKET_ERROR) {
    utils::print_wsa_error("[winsock error] listen failed");
    return false;
  }

  return true;
}

Client::Client() = default;

Client::~Client() {
  if (connection != nullptr) {
    ::shutdown(connection->socket, SD_SEND);
  }
}

auto Client::connect(ConnectionHandler &connection_handler, std::string ip, std::string port) -> bool {
  // resolve the server address and port
  auto addr_info = PADDRINFOA{};
  auto addr_hints = addrinfo{};
  addr_hints.ai_family = AF_UNSPEC;
  addr_hints.ai_socktype = SOCK_STREAM;
  addr_hints.ai_protocol = IPPROTO_TCP;

  auto getaddr_result = ::getaddrinfo(ip.data(), port.data(), &addr_hints, &addr_info);
  if (getaddr_result != 0) {
    std::cerr << std::format("[winsock error] getaddrinfo failed (error code: {})\n", getaddr_result);
    return false;
  }
  defer([&]() { ::freeaddrinfo(addr_info); });

  // attempt to connect to an address until one succeeds
  auto connect_socket = INVALID_SOCKET;
  auto ptr = addr_info;
  for (; ptr != nullptr; ptr = ptr->ai_next) {
    // create a socket for connecting to server
    connect_socket = ::WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (connect_socket == INVALID_SOCKET) {
      utils::print_wsa_error("[winsock error] socket creation failed");
      return false;
    }

    // try connect
    if (::WSAConnect(connect_socket, ptr->ai_addr, (int)ptr->ai_addrlen, nullptr, nullptr, nullptr, nullptr) ==
        SOCKET_ERROR) {
      ::closesocket(connect_socket);
      connect_socket = INVALID_SOCKET;
      continue;
    }

    break;
  }

  if (connect_socket == INVALID_SOCKET) {
    return false;
  }

  FD_SET(connect_socket, &connection_handler.read_set);
  FD_SET(connect_socket, &connection_handler.write_set);

  auto conn = Connection{connect_socket, *(SOCKADDR_IN *)ptr->ai_addr};
  connections.insert({connect_socket, conn});
  connection = &connections.at(connect_socket);
  connection_handler.cb.on_conn_started(this, *connection);
  return true;
}

auto Client::disconnect(ConnectionHandler &connection_handler) -> void {
  if (connection == nullptr) {
    return;
  }

  FD_ZERO(&connection_handler.read_set);
  FD_ZERO(&connection_handler.write_set);
  connections.clear();

  if (::shutdown(connection->socket, SD_SEND) == SOCKET_ERROR) {
    utils::print_wsa_error("[winsock error] shutdown failed");
  }

  connection_handler.cb.on_conn_ended(this, *connection);
}

ConnectionHandler::ConnectionHandler(NetEntity *net_entity) : net_entity(net_entity), cb(net_entity->base_callbacks) {
  FD_ZERO(&write_set);
  FD_ZERO(&read_set);
  FD_ZERO(&err_set);

  if (auto server = dynamic_cast<Server *>(net_entity)) {
    cb.on_select_error = [server](auto, auto &connection_handler, int err_code) {
      server->cb.on_select_error(server, connection_handler, err_code);
    };
    cb.on_select_timeout = [server](auto, auto &connection_handler) {
      server->cb.on_select_timeout(server, connection_handler);
    };
    cb.on_conn_accept_error = [server](auto, int err_code) {
      server->cb.on_conn_accept_error(server, err_code);
    };
    cb.on_conn_started = [server](auto, Connection &conn) {
      server->cb.on_conn_started(server, conn);
    };
    cb.on_conn_ended = [server](auto, Connection &conn) {
      server->cb.on_conn_ended(server, conn);
    };
    cb.on_recv_error = [server](auto, Connection &conn, int err_code) {
      server->cb.on_recv_error(server, conn, err_code);
    };
    cb.on_recv_success = [server](auto, Connection &conn) {
      server->cb.on_recv_success(server, conn);
    };
    cb.on_send_error = [server](auto, Connection &conn, int err_code) {
      server->cb.on_send_error(server, conn, err_code);
    };
    cb.on_send_success = [server](auto, Connection &conn) {
      server->cb.on_send_success(server, conn);
    };
  }

  if (auto client = dynamic_cast<Client *>(net_entity)) {
    cb.on_select_error = [client](auto, auto &connection_handler, int err_code) {
      client->cb.on_select_error(client, connection_handler, err_code);
    };
    cb.on_select_timeout = [client](auto, auto &connection_handler) {
      client->cb.on_select_timeout(client, connection_handler);
    };
    cb.on_conn_accept_error = [client](auto, int err_code) {
      client->cb.on_conn_accept_error(client, err_code);
    };
    cb.on_conn_started = [client](auto, Connection &conn) {
      client->cb.on_conn_started(client, conn);
    };
    cb.on_conn_ended = [client](auto, Connection &conn) {
      client->cb.on_conn_ended(client, conn);
    };
    cb.on_recv_error = [client](auto, Connection &conn, int err_code) {
      client->cb.on_recv_error(client, conn, err_code);
    };
    cb.on_recv_success = [client](auto, Connection &conn) {
      client->cb.on_recv_success(client, conn);
    };
    cb.on_send_error = [client](auto, Connection &conn, int err_code) {
      client->cb.on_send_error(client, conn, err_code);
    };
    cb.on_send_success = [client](auto, Connection &conn) {
      client->cb.on_send_success(client, conn);
    };
  }
}

auto ConnectionHandler::init() -> void {
  if (auto server = dynamic_cast<Server *>(net_entity)) {
    FD_SET(server->listen_socket, &read_set);
  }
}

auto ConnectionHandler::is_full() -> bool {
  return write_set.fd_count == sizeof(write_set.fd_array);
}

auto ConnectionHandler::tick(timeval timeout) -> bool {
  // select function will remove unavalible sockets in from the set
  // we need to copy the set to keep sockets in the set
  auto cur_read_set = read_set;
  auto cur_write_set = write_set;
  // auto cur_err_set = err_set;

  const auto select_result = ::select(0, &cur_read_set, &cur_write_set, nullptr, &timeout);

  // check select error
  if (select_result == SOCKET_ERROR) {
    const auto err_code = ::WSAGetLastError();
    utils::print_wsa_error("[winsock error] select failed", err_code);
    cb.on_select_error(net_entity, *this, err_code);
    return false;
  }

  // check select timeout
  if (select_result == 0) {
    cb.on_select_timeout(net_entity, *this);
    return true;
  }

  // loop over cur_read_set
  for (const auto sock : std::span{cur_read_set.fd_array, cur_read_set.fd_count}) {
    if (auto server = dynamic_cast<Server *>(net_entity)) {
      if (sock == server->listen_socket) {
        if (is_full()) {
          continue;
        }

        // listen socket accept
        auto accept_info = sockaddr_in{};
        auto accept_info_size = static_cast<int>(sizeof(accept_info));
        auto accept_socket =
          ::accept(server->listen_socket, std::bit_cast<sockaddr *>(&accept_info), &accept_info_size);
        if (accept_socket == INVALID_SOCKET) {
          const int err_code = ::WSAGetLastError();
          utils::print_wsa_error("[winsock error] accept failed", err_code);
          cb.on_conn_accept_error(net_entity, err_code);
          continue;
        }

        auto conn = Connection{accept_socket, accept_info};
        FD_SET(conn.socket, &read_set);
        FD_SET(conn.socket, &write_set);
        net_entity->connections.insert({conn.socket, conn});
        cb.on_conn_started(net_entity, net_entity->connections.at(conn.socket));
        continue;
      }
    }

    if (!net_entity->connections.contains(sock)) {
      // skip removed socket
      continue;
    }

    // handle connections
    auto &conn = net_entity->connections.at(sock);

    // alloc buffer
    if (conn.recv_total_size == 0) {
      // for packet header
      conn.is_recv_header = true;
      const auto header_size = sizeof(uint32_t);
      if (conn.recv_buf.size() < header_size) {
        conn.recv_buf.resize(header_size);
      }
      conn.recv_total_size = header_size;
    } else {
      // for packet body
      conn.is_recv_header = false;
      if (conn.recv_buf.size() < conn.recv_total_size) {
        conn.recv_buf.resize(conn.recv_total_size);
      }
    }

    // recv data
    auto wsa_buf = WSABUF{
      .len = conn.recv_total_size - conn.cur_recv_amount,
      .buf = conn.recv_buf.data() + conn.cur_recv_amount,
    };
    auto recv_len = u_long{0};
    auto recv_flags = u_long{0};
    const auto recv_result = ::WSARecv(conn.socket, &wsa_buf, 1ul, &recv_len, &recv_flags, nullptr, nullptr);
    if (recv_result == SOCKET_ERROR) {
      const auto err_code = ::WSAGetLastError();
      conn.close();
      FD_CLR(conn.socket, &read_set);
      FD_CLR(conn.socket, &write_set);
      cb.on_recv_error(net_entity, conn, err_code);
      cb.on_conn_ended(net_entity, conn);
      net_entity->connections.erase(conn.socket);
    } else {
      if (recv_len == 0) {
        conn.close();
        FD_CLR(conn.socket, &read_set);
        FD_CLR(conn.socket, &write_set);
        cb.on_conn_ended(net_entity, conn);
        net_entity->connections.erase(conn.socket);
      } else {
        conn.cur_recv_amount += recv_len;
        // std::cout << "[reciving] " << (conn.is_recv_header ? "header" : "body")
        //           << std::format(" -> progress: {}/{}\n", conn.cur_recv_amount, conn.recv_total_size);

        if (conn.cur_recv_amount == conn.recv_total_size) {
          if (conn.is_recv_header) {
            conn.recv_total_size = *std::bit_cast<uint32_t *>(conn.recv_buf.data());
            conn.is_recv_header = false;
          } else {
            // on packet body recv finish
            cb.on_recv_success(net_entity, conn);
            conn.recv_total_size = 0;
            conn.is_recv_header = true;
          }
          conn.recv_buf.clear();
          conn.cur_recv_amount = 0;
        }
      }
    }
  }

  // loop over cur_write_set
  for (const auto sock : std::span{cur_write_set.fd_array, cur_write_set.fd_count}) {
    if (auto server = dynamic_cast<Server *>(net_entity)) {
      if (sock == server->listen_socket) {
        // skip listen socket
        continue;
      }
    }

    if (!net_entity->connections.contains(sock)) {
      // skip removed socket
      continue;
    }

    // handle connections
    auto &conn = net_entity->connections.at(sock);

    // send data
    if (!conn.send_queue.is_empty()) {
      if (conn.send_buf.empty()) {
        conn.send_buf = conn.send_queue.pop_front();
      }

      if (conn.cur_send_amount < conn.send_buf.size()) {
        auto wsa_buf = WSABUF{
          .len = static_cast<u_long>(conn.send_buf.size() - conn.cur_send_amount),
          .buf = conn.send_buf.data() + conn.cur_send_amount,
        };
        auto send_len = u_long{0};
        const auto send_result = ::WSASend(conn.socket, &wsa_buf, 1ul, &send_len, 0, nullptr, nullptr);
        if (send_result == SOCKET_ERROR) {
          const auto err_code = ::WSAGetLastError();
          conn.close();
          FD_CLR(conn.socket, &read_set);
          FD_CLR(conn.socket, &write_set);
          cb.on_send_error(net_entity, conn, err_code);
          cb.on_conn_ended(net_entity, conn);
          net_entity->connections.erase(conn.socket);
        } else {
          conn.cur_send_amount += send_len;
          // std::cout << std::format("[sending] -> progress: {}/{}\n", send_result, conn.send_buf.size());

          if (conn.cur_send_amount == conn.send_buf.size()) {
            // packet recive finish
            cb.on_send_success(net_entity, conn);
            conn.send_buf.clear();
            conn.cur_send_amount = 0;
          }
        }
      }
    }
  }

  return true;
}

auto ConnectionHandler::run(timeval timeout, bool &stop_flag) -> bool {
  while (!stop_flag) {
    if (!tick(timeout)) {
      return false;
    }
  }

  return true;
}

auto ConnectionHandler::run(timeval timeout, std::atomic_bool &stop_flag) -> bool {
  while (!stop_flag.load()) {
    if (!tick(timeout)) {
      return false;
    }
  }

  return true;
}

} // namespace winnet
