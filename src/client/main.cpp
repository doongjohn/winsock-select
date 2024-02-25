#define WIN32_LEAN_AND_MEAN

#include <chrono>
#include <future>
#include <format>

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
  if (!winnet::wsa_init()) {
    return EXIT_FAILURE;
  }
  defer(winnet::wsa_deinit);

  auto client = new winnet::Client{};
  defer([=] { delete (client); });

  auto stop_flag = std::atomic_bool{false};
  auto conn_handler = winnet::ConnectionHandler{client};
  conn_handler.init();

  auto screen = ftxui::ScreenInteractive::FullscreenAlternateScreen();
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

  client->cb.on_conn_started = [&](winnet::Client *, winnet::Connection &) {
    screen.Post([&]() {
      message_list.emplace_back(std::format("서버에 접속되었습니다. ({}:{})\n", SERVER_IP, SERVER_PORT));
      selected_msg = static_cast<int>(message_list.size() - 1);
      screen.PostEvent(ftxui::Event::Custom);
    });
  };

  client->cb.on_conn_ended = [&](winnet::Client *, winnet::Connection &) {
    screen.Post([&]() {
      message_list.emplace_back("서버와 접속이 끊겼습니다.");
      selected_msg = static_cast<int>(message_list.size() - 1);
      screen.PostEvent(ftxui::Event::Custom);
    });
  };

  client->cb.on_recv_success = [&](winnet::Client *, winnet::Connection &conn) {
    screen.Post([&, recv_string = conn.get_recv_string()]() {
      message_list.push_back(recv_string);
      selected_msg = static_cast<int>(message_list.size() - 1);
      screen.PostEvent(ftxui::Event::Custom);
    });
  };

  client->cb.on_recv_error = [](winnet::Client *, winnet::Connection &, int err_code) {
    utils::print_wsa_error("recv error", err_code);
  };

  auto fut_tick = std::async(std::launch::async, [&]() {
    // wait for fixui screen loop to start
    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    while (!client->connect(conn_handler, SERVER_IP, SERVER_PORT)) {
      screen.Post([&]() {
        message_list.emplace_back("서버에 접속중...");
        selected_msg = static_cast<int>(message_list.size() - 1);
        screen.PostEvent(ftxui::Event::Custom);
      });
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    const auto timeout = timeval{
      .tv_sec = 1,
      .tv_usec = 0,
    };

    if (!conn_handler.run(timeout, stop_flag)) {
      stop_flag.store(true);
    }
  });

  auto container = ftxui::Container::Vertical({
    messages,
    textarea,
  });

  textarea->TakeFocus();

  screen.Loop(ftxui::Renderer(container, [&] {
    return ftxui::vbox({
             ftxui::text("채팅 서버") | ftxui::center,
             ftxui::separator(),
             messages->Render() | ftxui::focusPositionRelative(0, 1) | ftxui::yframe | ftxui::flex,
             ftxui::separator(),
             textarea->Render(),
           }) |
           ftxui::border;
  }));

  stop_flag.store(true);
  fut_tick.wait();

  return EXIT_SUCCESS;
}
