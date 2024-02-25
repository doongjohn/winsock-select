#define WIN32_LEAN_AND_MEAN

#include <array>
#include <format>
#include <iostream>

#include <utils.hpp>
#include <winnet.hpp>

auto main() -> int {
  if (!winnet::wsa_init()) {
    return EXIT_FAILURE;
  }
  defer(winnet::wsa_deinit);

  auto server = new winnet::Server{};
  defer([=] { delete (server); });

  if (!server->init(8000)) {
    return EXIT_FAILURE;
  }

  if (!server->listen()) {
    return EXIT_FAILURE;
  }

  auto stop_flag = false;
  auto conn_handler = winnet::ConnectionHandler{server};
  conn_handler.init();

  server->cb.on_conn_started = [](winnet::Server *, winnet::Connection &conn) {
    std::cout << std::format("client connected: {:016X}\n", conn.socket);
    conn.send("[서버] 당신의 이름을 입력해주세요.");
  };

  server->cb.on_conn_ended = [](winnet::Server *server, winnet::Connection &conn) {
    std::cout << std::format("client disconnected: {:016X}\n", conn.socket);
    server->send_all(std::format("[서버] {}의 접속이 끊겼습니다.", conn.username));
  };

  server->cb.on_recv_success = [](winnet::Server *server, winnet::Connection &conn) {
    const auto recv_string = conn.get_recv_string();
    std::cout << std::format("recv: {}\n", recv_string);

    if (conn.username.empty()) {
      conn.username = recv_string;
      conn.send(std::format("[서버] 당신의 이름은 {} 입니다.", recv_string));

      auto ignore_socket = std::array{conn.socket};
      server->send_all_but(ignore_socket, std::format("[서버] {}가 접속했습니다.", conn.username));
    } else {
      server->send_all(std::format("{}: {}", conn.username, recv_string));
    }
  };

  server->cb.on_recv_error = [](winnet::Server *, winnet::Connection &conn, int err_code) {
    std::cerr << std::format("recv error: {:016X} (error code: {})\n", conn.socket, err_code);
  };

  const auto timeout = timeval{
    .tv_sec = 1,
    .tv_usec = 0,
  };

  std::cout << "server started\n";
  if (!conn_handler.run(timeout, stop_flag)) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
