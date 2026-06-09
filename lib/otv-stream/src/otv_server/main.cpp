/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <csignal>
#include <cstring>
#include <filesystem>

#include <cxxopts.hpp>
#include <fmt/core.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <otv_sdk/engine.hpp>

#include "server.hpp"

int main(int argc, char **argv)
{
    try {
        cxxopts::Options options("otv-server", "OTV libcamera capture + analysis server");
        options.add_options()
            ("c,config", "Path to OTV TOML config file",
             cxxopts::value<std::string>()->default_value("otv_config.toml"))
            ("h,help", "Show help");

        cxxopts::ParseResult args;
        args = options.parse(argc, argv);

        if (args.count("help")) {
            fmt::println("{}", options.help());
            return 0;
        }

        spdlog::stdout_color_mt("console");
        otv_sdk::Config otv_config;

        auto config_path = std::filesystem::path(args["config"].as<std::string>());
        otv_config  = otv_sdk::Config::from_file(config_path);
        spdlog::info("loaded OTV config from {}", config_path.string());

        RequestQueue queue = {};
        Rpi4BCamera rpi_cam = Rpi4BCamera(otv_config, queue);

        std::atomic<bool> stop_flag{false};
        static std::atomic<bool>* stop_ptr = &stop_flag;
        std::signal(SIGINT,  [](int) { stop_ptr->store(true); });
        std::signal(SIGTERM, [](int) { stop_ptr->store(true); });

        auto analyzer = OtvAnalyzer(otv_config, rpi_cam, stop_flag);
        analyzer.run();
        analyzer.log_results();

        return 0;
    } catch (const spdlog::spdlog_ex &err) {
        fmt::println(stderr, "logger init failed: {}", err.what());
        return 1;
    } catch (const Rpi4BCameraError &err) {
        fmt::println(stderr, "rpi 4B camera error: {}", err.what());
        return 1;
    } catch (const cxxopts::exceptions::exception& e) {
        fmt::println(stderr, "cli error: {}", e.what());
        return 1;
    } catch (const otv_sdk::ConfigError& e) {
        spdlog::error("Config parse error: {}", e.what());
        return 1;
    } catch (const std::exception& e) {
        spdlog::error("Failed to load config: {}", e.what());
        return 1;
    }
}
