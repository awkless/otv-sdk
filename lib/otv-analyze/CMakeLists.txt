# SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
# SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(otv-analyze)

target_sources(
    otv-analyze
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp"
)

target_link_libraries(
    otv-analyze
    PRIVATE
        cxxopts::cxxopts OtvSdk::Engine fmt::fmt
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
   target_compile_options(
      otv-analyze
      PRIVATE
         $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic>
   )
endif()
