#include "utils.hpp"

#include <io.h>
#include <windows.h>

#include <format>
#include <fcntl.h>
#include <iostream>

namespace utils {

auto console_read_line() -> std::string {
  // vscode terminal에서만 한글 입력이 안되는 문제가 있음
  // wezterm, windows terminal에서는 그냥 getline(std::cin, input) 해도 한국어 입력 잘됨

  _setmode(_fileno(stdin), _O_U16TEXT);
  std::wstring input_utf16;
  std::getline(std::wcin, input_utf16);
  _setmode(_fileno(stdin), _O_TEXT);

  const auto utf16_len = static_cast<int>(input_utf16.length());
  const auto utf8_len = ::WideCharToMultiByte(CP_UTF8, 0, input_utf16.data(), utf16_len, nullptr, 0, nullptr, nullptr);

  auto input_utf8 = std::string(utf8_len, '\0');
  ::WideCharToMultiByte(CP_UTF8, 0, input_utf16.data(), utf16_len, input_utf8.data(), utf8_len, nullptr, nullptr);
  return input_utf8;
}

auto print_wsa_error(std::string msg, int err_code, const std::source_location &src_loc) -> void {
  const auto file_name = src_loc.file_name();
  const auto line = src_loc.line();
  const auto column = src_loc.column();
  const auto function_name = src_loc.function_name();
  std::cerr << std::format("{} (error code: {})\n└>called from `{}` {}:{}:{}\n", msg, err_code, function_name,
                           file_name, line, column);
}

auto print_wsa_error(std::string msg, const std::source_location &src_loc) -> void {
  auto err_code = ::WSAGetLastError();
  print_wsa_error(std::move(msg), err_code, src_loc);
}

auto addr_to_string(IN_ADDR addr) -> std::string {
  char ip_str[INET_ADDRSTRLEN];
  ::inet_ntop(AF_INET, &addr, ip_str, INET_ADDRSTRLEN);
  return ip_str;
}

} // namespace utils
