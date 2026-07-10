# SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
# SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
# SPDX-License-Identifier: GPL-3.0-or-later

if(TARGET_RPI4B)
    add_executable(otv-server)

    target_sources(
        otv-server
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/otv_server/main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/otv_server/server.cpp"
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBCAMERA REQUIRED libcamera)

    target_link_libraries(
        otv-server
        PRIVATE
        cxxopts::cxxopts spdlog::spdlog OtvSdk::Engine fmt::fmt ${LIBCAMERA_LIBRARIES}
    )

    target_include_directories(
        otv-server
        PRIVATE
            ${websocketpp_SOURCE_DIR}
            ${asio_SOURCE_DIR}/asio/include
            ${LIBCAMERA_INCLUDE_DIRS}
    )

    target_compile_definitions(otv-server PRIVATE ASIO_STANDALONE)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
          otv-server
          PRIVATE
              $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic -Wno-template-id-cdtor>
        )
    endif()

    install(TARGETS otv-server DESTINATION bin)
else()
    add_executable(otv-client)

    target_sources(
        otv-client
        PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/otv_client/main.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/src/otv_client/client.cpp"
    )

    target_link_libraries(
        otv-client
        PRIVATE
            cxxopts::cxxopts fmt::fmt ${OpenCV_LIBS}
    )

    target_include_directories(
        otv-client
        PRIVATE
            ${websocketpp_SOURCE_DIR}
            ${asio_SOURCE_DIR}/asio/include
            ${OpenCV_INCLUDE_DIRS}
    )

    target_compile_definitions(otv-client PRIVATE ASIO_STANDALONE)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(
          otv-client
          PRIVATE
              $<$<COMPILE_LANGUAGE:CXX>:-Wall -pedantic -Wno-template-id-cdtor>
        )
    endif()

    install(TARGETS otv-client DESTINATION bin)
endif()
