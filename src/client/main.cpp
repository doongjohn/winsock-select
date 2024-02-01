#define WIN32_LEAN_AND_MEAN

#include <chrono>
#include <future>
#include <format>
#include <iostream>

#include <ftxui/component/captured_mouse.hpp>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <utils.hpp>
#include <winnet.hpp>

#define SERVER_IP "localhost"
#define SERVER_PORT "8000"

auto main() -> int {
  // init winsock
  if (!winnet::wsa_init()) {
    return EXIT_FAILURE;
  }
  defer(winnet::wsa_deinit);

  auto client = new winnet::Client{};
  if (!client->init()) {
    return EXIT_FAILURE;
  }

  auto stop_signal = std::atomic_bool{false};
  auto conn_handler = winnet::ConnectionHandler{client};
  conn_handler.init();

  auto screen = ftxui::ScreenInteractive::Fullscreen();
  auto message_list = std::vector<std::string>{};
  auto selected_msg = 0;

  auto messages = ftxui::Menu({
    .entries = &message_list,
    .selected = &selected_msg,
  });

  auto text_input = std::string{};
  auto input_option = ftxui::InputOption{};
  input_option.multiline = false;
  input_option.on_enter = [&]() {
    client->connection->send(text_input);
    text_input = "";
  };
  auto textarea = ftxui::Input(&text_input, input_option);

  auto container = ftxui::Container::Vertical({
    messages,
    textarea,
  });

  client->callbacks.on_conn_connected = [&](winnet::Client *, winnet::Connection &) {
    std::cout << std::format("connected: {}:{}\n", SERVER_IP, SERVER_PORT);
  };

  client->callbacks.on_recv_error = [](winnet::Client *, winnet::Connection &, int err_code) {
    utils::print_wsa_error("recv error", err_code);
  };

  client->callbacks.on_recv_success = [&](winnet::Client *, winnet::Connection &conn) {
    screen.Post([&, recv_string = conn.get_recv_string()]() {
      message_list.push_back(recv_string);
      selected_msg = static_cast<int>(message_list.size() - 1);
      screen.PostEvent(ftxui::Event::Custom);
    });
  };

  std::cout << "connecting to server...\n";
  while (!client->connect(conn_handler, SERVER_IP, SERVER_PORT)) {
    std::cerr << "retrying...\n";
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }

  std::cout << "client started\n";

  auto fut_tick = std::async(std::launch::async, [&]() {
    auto timeout = timeval{
      .tv_sec = 1,
      .tv_usec = 0,
    };
    if (!conn_handler.run(timeout, stop_signal)) {
      stop_signal.store(true);
    }
  });

  auto component = ftxui::Renderer(container, [&] {
    return ftxui::vbox({
             ftxui::text("채팅 서버") | ftxui::center,
             ftxui::separator(),
             messages->Render() | ftxui::focusPositionRelative(0, 1) | ftxui::yframe | ftxui::flex,
             ftxui::separator(),
             textarea->Render(),
           }) |
           ftxui::border;
  });

  textarea->TakeFocus();

  screen.Loop(component);
  stop_signal.store(true);

  std::cout << "client closed\n";
  return EXIT_SUCCESS;
}
