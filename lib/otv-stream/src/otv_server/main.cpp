/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <csignal>
#include <filesystem>
#include <memory>
#include <string>

#include <cxxopts.hpp>
#include <fmt/core.h>
#include <opencv2/imgcodecs.hpp>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <otv_sdk/engine.hpp>

#include "server.hpp"

struct CliOptions {
    uint16_t port;
    uint16_t viz_port = 0;
    bool has_viz = false;
    std::filesystem::path config_path;
    std::filesystem::path mask_path;
    bool calibrate = false;
};

static CliOptions
parse_cli(int argc, char** argv)
{
    cxxopts::Options opts("otv-server", "OTV libcamera capture + analysis server");
    opts.add_options()("c,calibrate",
        "Enter calibration mode (streams raw camera frames; disables OTV analysis)")("m,mask",
        "Apply mask image during OTV analysis (0=exclude, 255=include)",
        cxxopts::value<std::string>())("viz-port",
        "WebSocket port for velocity visualization frames (analysis mode only)",
        cxxopts::value<uint16_t>())("h,help", "Print help message")(
        "port", "WebSocket port number", cxxopts::value<uint16_t>())(
        "config", "Path to OTV TOML configuration file", cxxopts::value<std::string>());

    opts.parse_positional({ "port", "config" });
    opts.positional_help("<port> <config_file>");

    try {
        auto args = opts.parse(argc, argv);

        if (args.count("help")) {
            fmt::println("{}", opts.help());
            std::exit(0);
        }

        if (!args.count("port") || !args.count("config")) {
            fmt::println(stderr, "Error: <port> and <config_file> are required");
            fmt::println(stderr, "{}", opts.help());
            std::exit(1);
        }

        CliOptions result;
        result.port = args["port"].as<uint16_t>();
        result.config_path = args["config"].as<std::string>();
        result.calibrate = args.count("calibrate") > 0;

        if (args.count("mask"))
            result.mask_path = args["mask"].as<std::string>();

        if (args.count("viz-port")) {
            result.viz_port = args["viz-port"].as<uint16_t>();
            result.has_viz = true;
        }

        return result;
    } catch (const cxxopts::exceptions::exception& e) {
        fmt::println(stderr, "CLI error: {}", e.what());
        std::exit(1);
    }
}

int
main(int argc, char** argv)
{
    try {
        spdlog::stdout_color_mt("console");

        CliOptions cli = parse_cli(argc, argv);

        otv_sdk::Config otv_config = otv_sdk::Config::from_file(cli.config_path);
        spdlog::info("loaded OTV config from {}", cli.config_path.string());

        OtvWebSocketServer ws_server(cli.port);
        ws_server.start();

        std::unique_ptr<OtvWebSocketServer> viz_ws;
        if (cli.has_viz) {
            viz_ws = std::make_unique<OtvWebSocketServer>(cli.viz_port);
            viz_ws->start();
            spdlog::info("viz WebSocket server listening on port {}", cli.viz_port);
        }

        cv::Mat mask;
        if (!cli.mask_path.empty()) {
            mask = cv::imread(cli.mask_path.string(), cv::IMREAD_GRAYSCALE);
            if (mask.empty()) {
                spdlog::error("could not load mask '{}'", cli.mask_path.string());
                if (viz_ws)
                    viz_ws->stop();
                ws_server.stop();
                return 1;
            }
            spdlog::info("mask loaded from '{}'", cli.mask_path.string());
        }

        RequestQueue queue;
        Rpi4BCamera rpi_cam(otv_config, queue);

        std::atomic<bool> stop_flag { false };
        static std::atomic<bool>* stop_ptr = &stop_flag;
        std::signal(SIGINT, [](int) { stop_ptr->store(true); });
        std::signal(SIGTERM, [](int) { stop_ptr->store(true); });

        if (cli.calibrate) {
            spdlog::info("entering calibration mode");
            CalibrationStreamer streamer(rpi_cam, ws_server, stop_flag, mask);
            streamer.run();
        } else {
            OtvAnalyzer analyzer(otv_config, rpi_cam, ws_server, viz_ws.get(), stop_flag);
            if (!mask.empty())
                analyzer.set_mask(mask);
            analyzer.run();
            analyzer.log_results();
        }

        if (viz_ws)
            viz_ws->stop();
        ws_server.stop();
        return 0;
    } catch (const spdlog::spdlog_ex& err) {
        fmt::println(stderr, "logger init failed: {}", err.what());
        return 1;
    } catch (const Rpi4BCameraError& err) {
        spdlog::error("rpi 4B camera error: {}", err.what());
        return 1;
    } catch (const otv_sdk::ConfigError& err) {
        spdlog::error("config parse error: {}", err.what());
        return 1;
    } catch (const std::filesystem::filesystem_error& err) {
        spdlog::error("filesystem error: {}", err.what());
        return 1;
    } catch (const std::exception& err) {
        spdlog::error("error: {}", err.what());
        return 1;
    }
}
