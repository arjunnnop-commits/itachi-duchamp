cmake_minimum_required(VERSION 3.22)
project(itachid CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(itachid
  src/main.cpp
  src/config.cpp
  src/remote.cpp
  src/xiaomi.cpp
  src/fod.cpp
  src/aod.cpp
  src/backend.cpp
  src/panel_service.cpp
  src/auth.cpp)

# 64 hex chars. Empty = insecure dev default (anyone can derive the license). CI passes a real one.
set(ITACHI_SECRET_HEX "" CACHE STRING "Per-owner license secret (hex)")
if(ITACHI_SECRET_HEX)
  target_compile_definitions(itachid PRIVATE ITACHI_SECRET_HEX="${ITACHI_SECRET_HEX}")
else()
  message(WARNING "ITACHI_SECRET_HEX not set: using the insecure development secret")
endif()
target_compile_options(itachid PRIVATE -Wall -Wextra)
# libbinder_ndk is a platform library; the NDK ships a link stub for API >= 29.
target_link_libraries(itachid PRIVATE binder_ndk log)
