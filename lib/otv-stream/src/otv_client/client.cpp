/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <fmt/core.h>

#include "client.hpp"

OtvClient::OtvClient()
{
    m_endpoint.clear_access_channels(websocketpp::log::alevel::all);
    m_endpoint.clear_error_channels(websocketpp::log::elevel::all);
    m_endpoint.init_asio();
    m_endpoint.start_perpetual();
    m_thread = std::thread(&ws_client_t::run, &m_endpoint);
}

OtvClient::~OtvClient()
{
    m_endpoint.stop_perpetual();
    if (m_thread.joinable())
        m_thread.join();
}

void
OtvClient::connect(const std::string& uri, std::function<void(const std::string&)> on_text,
    std::function<void(const std::string&)> on_binary, std::function<void()> on_close)
{
    m_on_text = std::move(on_text);
    m_on_binary = std::move(on_binary);
    m_on_close = std::move(on_close);

    websocketpp::lib::error_code ec;
    ws_client_t::connection_ptr con = m_endpoint.get_connection(uri, ec);
    if (ec) {
        fmt::println(stderr, "connect error: {}", ec.message());
        if (m_on_close)
            m_on_close();
        return;
    }

    m_hdl = con->get_handle();

    con->set_open_handler([this](websocketpp::connection_hdl) { m_open.store(true); });

    con->set_close_handler([this](websocketpp::connection_hdl) {
        m_open.store(false);
        if (m_on_close)
            m_on_close();
    });

    con->set_fail_handler([this](websocketpp::connection_hdl hdl) {
        auto c = m_endpoint.get_con_from_hdl(hdl);
        fmt::println(stderr, "connection failed: {}", c->get_ec().message());
        m_open.store(false);
        if (m_on_close)
            m_on_close();
    });

    con->set_message_handler([this](websocketpp::connection_hdl, ws_client_t::message_ptr msg) {
        const auto op = msg->get_opcode();
        if (op == websocketpp::frame::opcode::text && m_on_text)
            m_on_text(msg->get_payload());
        else if (op == websocketpp::frame::opcode::binary && m_on_binary)
            m_on_binary(msg->get_payload());
    });

    m_endpoint.connect(con);
}

void
OtvClient::close()
{
    if (!m_open.load())
        return;
    websocketpp::lib::error_code ec;
    m_endpoint.close(m_hdl, websocketpp::close::status::normal, "client exiting", ec);
}
