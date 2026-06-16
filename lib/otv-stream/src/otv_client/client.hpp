/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OTV_STREAM_CLIENT_HPP
#define OTV_STREAM_CLIENT_HPP

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include <opencv2/core.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>

// SharedFrame — written by the WS thread, read by the main thread.
//
// Declare SharedFrame before OtvClient so that when both are local variables,
// SharedFrame outlives the client's background thread (LIFO destruction order).
struct SharedFrame {
    cv::Mat    mat;
    bool       updated = false;
    std::mutex mutex;
};

class OtvClient {
public:
    OtvClient();
    ~OtvClient();

    /// Connect to uri and register per-opcode callbacks.
    ///
    /// Callbacks fire on the endpoint thread; keep them short and lock shared
    /// state with a mutex. Pass nullptr for any opcode you do not need.
    /// on_close fires on connection close or failure.
    void connect(const std::string&                      uri,
                 std::function<void(const std::string&)> on_text,
                 std::function<void(const std::string&)> on_binary,
                 std::function<void()>                   on_close);

    /// Send a graceful close frame if the connection is open.
    void close();

private:
    using ws_client_t = websocketpp::client<websocketpp::config::asio_client>;

    ws_client_t                               m_endpoint;
    std::thread                               m_thread;
    websocketpp::connection_hdl               m_hdl;
    std::atomic<bool>                         m_open{false};
    std::function<void(const std::string&)>   m_on_text;
    std::function<void(const std::string&)>   m_on_binary;
    std::function<void()>                     m_on_close;
};

#endif /* OTV_STREAM_CLIENT_HPP */
