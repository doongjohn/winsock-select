#pragma once

#include <ws2tcpip.h>

#include <source_location>
#include <string>
#include <functional>

namespace utils {

#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a##b

struct DeferStruct {
  std::function<void()> defer_func;
  ~DeferStruct() {
    defer_func();
  }
};

#define defer(func) \
  utils::DeferStruct CONCAT(__defer_, __COUNTER__) { \
    .defer_func = (func), \
  };

auto console_read_line() -> std::string;

auto print_wsa_error(std::string msg, int error_code,
                     const std::source_location &src_loc = std::source_location::current()) -> void;

auto print_wsa_error(std::string msg, const std::source_location &src_loc = std::source_location::current()) -> void;

auto addr_to_string(IN_ADDR addr) -> std::string;

} // namespace utils
