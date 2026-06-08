# SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
# SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
# SPDX-License-Identifier: GPL-3.0-or-later

if(TARGET_RPI4B)
    add_executable(otv-server)

    target_sources(
        otv-server
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/otv_server/main.cpp"
    )

    target_link_libraries(
        otv-server
        PRIVATE
            cxxopts::cxxopts OtvSdk::Engine fmt::fmt
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
          otv-server
          PRIVATE
             $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic>
        )
    endif()

    install(TARGETS otv-server DESTINATION bin)
else()
    add_executable(otv-client)

    target_sources(
        otv-client
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/otv_client/main.cpp"
    )

    target_link_libraries(
        otv-client
        PRIVATE
            cxxopts::cxxopts
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
          otv-client
          PRIVATE
             $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic>
        )
    endif()

    install(TARGETS otv-client DESTINATION bin)
endif()
