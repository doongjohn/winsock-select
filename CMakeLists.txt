cmake_minimum_required(VERSION 3.27)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_COLOR_DIAGNOSTICS ON)

project(
  LearnServer
  VERSION 0.1.0
  DESCRIPTION ""
  HOMEPAGE_URL ""
  LANGUAGES CXX
)

# ===
# target: utils
# ===
add_library(utils OBJECT "")

target_compile_features(utils
  PRIVATE cxx_std_20)

file(GLOB SOURCES
  src/utils/*.cpp
  src/utils/*.hpp)
target_sources(utils
  PRIVATE ${SOURCES})

target_include_directories(utils
  PUBLIC src/utils)

# ===
# target: winnet
# ===
add_library(winnet OBJECT "")

target_compile_features(winnet
  PRIVATE cxx_std_20)

file(GLOB SOURCES
  src/winnet/*.cpp
  src/winnet/*.hpp)
target_sources(winnet
  PRIVATE ${SOURCES})

target_include_directories(winnet
  PUBLIC src/winnet)

target_link_libraries(winnet
  PUBLIC ws2_32 # <-- this is for winsock2
  PRIVATE utils)

# ===
# target: server
# ===
add_executable(server "")

set_target_properties(server
  PROPERTIES
  OUTPUT_NAME server)

target_compile_features(server
  PRIVATE cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(server
    # more warnings
    PRIVATE -Wall
    PRIVATE -Wextra)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(server
    # more warnings
    PRIVATE /Wall
    PRIVATE /sdl)
endif()

file(GLOB SOURCES
  src/server/*.cpp
  src/server/*.hpp)
target_sources(server
  PRIVATE ${SOURCES})

target_link_libraries(server
  PRIVATE utils
  PRIVATE winnet)

# ===
# target: client
# ===
add_executable(client "")

set_target_properties(client
  PROPERTIES
  OUTPUT_NAME client)

target_compile_features(client
  PRIVATE cxx_std_20)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(client
    # more warnings
    PRIVATE -Wall
    PRIVATE -Wextra)
endif()
if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  target_compile_options(client
    # more warnings
    PRIVATE /Wall
    PRIVATE /sdl)
endif()

file(GLOB SOURCES
  src/client/*.cpp
  src/client/*.hpp)
target_sources(client
  PRIVATE ${SOURCES})

add_subdirectory("./vendor/FTXUI")

target_link_libraries(client
  PRIVATE utils
  PRIVATE winnet
  PRIVATE ftxui::screen
  PRIVATE ftxui::dom
  PRIVATE ftxui::component)
