/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <cxxopts.hpp>
#include <fmt/core.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgcodecs.hpp>

#include "client.hpp"

struct CliOptions {
    std::string host;
    uint16_t    port;
    bool        calibrate = false;
};

static CliOptions
parse_cli(int argc, char** argv)
{
    cxxopts::Options opts("otv-client", "OTV WebSocket client");
    opts.add_options()
        ("c,calibrate", "Enter calibrate mode (display raw camera frames)")
        ("h,help",      "Display help message")
        ("host", "Server IP address",    cxxopts::value<std::string>())
        ("port", "WebSocket port number", cxxopts::value<uint16_t>());

    opts.parse_positional({"host", "port"});
    opts.positional_help("<host> <port>");

    try {
        auto args = opts.parse(argc, argv);

        if (args.count("help")) {
            fmt::println("{}", opts.help());
            std::exit(0);
        }

        if (!args.count("host") || !args.count("port")) {
            fmt::println(stderr, "Error: <host> and <port> are required");
            fmt::println(stderr, "{}", opts.help());
            std::exit(1);
        }

        CliOptions result;
        result.host      = args["host"].as<std::string>();
        result.port      = args["port"].as<uint16_t>();
        result.calibrate = args.count("calibrate") > 0;
        return result;
    } catch (const cxxopts::exceptions::exception& e) {
        fmt::println(stderr, "CLI error: {}", e.what());
        std::exit(1);
    }
}

static void
print_stats(const std::string& payload)
{
    int   frame = 0, count = 0;
    float avg = 0.0f, vmax = 0.0f, vmin = 0.0f, std_dev = 0.0f;

    if (std::sscanf(payload.c_str(),
            "{\"frame\":%d,\"avg\":%f,\"max\":%f,\"min\":%f,\"std_dev\":%f,\"count\":%d}",
            &frame, &avg, &vmax, &vmin, &std_dev, &count) != 6) {
        fmt::println(stderr, "\nmalformed message: {}", payload);
        return;
    }

    fmt::print("\r[frame {:6d}]  avg={:.4f} m/s  max={:.4f} m/s  "
               "min={:.4f} m/s  std_dev={:.4f} m/s  n={:5d}     ",
               frame, avg, vmax, vmin, std_dev, count);
    std::fflush(stdout);
}

int main(int argc, char** argv)
{
    auto cli = parse_cli(argc, argv);
    const std::string uri = fmt::format("ws://{}:{}", cli.host, cli.port);

    std::atomic<bool> stop_flag{false};
    static std::atomic<bool>* stop_ptr = &stop_flag;
    std::signal(SIGINT,  [](int) { stop_ptr->store(true); });
    std::signal(SIGTERM, [](int) { stop_ptr->store(true); });

    if (cli.calibrate) {
        // Declared before OtvClient so it outlives the client thread (LIFO).
        SharedFrame shared;
        OtvClient client;

        client.connect(uri,
            nullptr,
            [&shared](const std::string& payload) {
                std::vector<uchar> jpeg(payload.begin(), payload.end());
                cv::Mat frame = cv::imdecode(jpeg, cv::IMREAD_GRAYSCALE);
                if (frame.empty())
                    return;
                std::lock_guard<std::mutex> lock(shared.mutex);
                frame.copyTo(shared.mat);
                shared.updated = true;
            },
            [&stop_flag]() { stop_flag.store(true); });

        fmt::println("calibrate mode — connecting to {}", uri);
        fmt::println("press 'q' or ESC to quit");
        cv::namedWindow("OTV Calibration", cv::WINDOW_NORMAL);

        while (!stop_flag.load()) {
            {
                std::lock_guard<std::mutex> lock(shared.mutex);
                if (shared.updated && !shared.mat.empty()) {
                    cv::imshow("OTV Calibration", shared.mat);
                    shared.updated = false;
                }
            }
            int key = cv::waitKey(30);
            if (key == 'q' || key == 27)
                stop_flag.store(true);
        }

        cv::destroyAllWindows();
        client.close();
    } else {
        OtvClient client;

        fmt::println("OTV result mode — connecting to {}", uri);
        client.connect(uri,
            [](const std::string& payload) { print_stats(payload); },
            nullptr,
            [&stop_flag]() {
                fmt::println("\nserver closed connection");
                stop_flag.store(true);
            });

        while (!stop_flag.load())
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

        fmt::println("");
        client.close();
    }

    return 0;
}
