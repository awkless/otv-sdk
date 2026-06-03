/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <cxxopts.hpp>
#include <fmt/core.h>
#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <otv_sdk/engine.hpp>

static void
write_stats(const std::filesystem::path& output_dir,
            const otv_sdk::VelocityStats& stats,
            float elapsed_seconds)
{
    std::filesystem::create_directories(output_dir);
    std::filesystem::path file_path = output_dir / "stats_synthesis.csv";
    std::ofstream f(file_path);

    if (!f.is_open()) {
        fmt::println(stderr, "Failed to open {} for writing", file_path.string());
        std::exit(1);
    }

    f << "avg," << stats.avg << '\n'
      << "min velocity," << stats.min << '\n'
      << "max velocity," << stats.max << '\n'
      << "std_dev," << stats.std_dev << '\n'
      << "tot_trajectories," << stats.count << '\n'
      << "time," << elapsed_seconds;
}

static void
write_sub_regions(const std::filesystem::path& output_dir,
                  const otv_sdk::SubRegion& sub_regions,
                  int width)
{
    std::filesystem::create_directories(output_dir);
    std::filesystem::path file_path = output_dir / "sub_regions.csv";
    std::ofstream f(file_path);

    if (!f.is_open()) {
        fmt::println(stderr, "Failed to open {} for writing", file_path.string());
        std::exit(1);
    }

    int step = sub_regions.step();
    size_t n = sub_regions.size();

    // INVARIANT: Write stats per sub-region column.
    f << "Interval, ";
    int boundary = 0;
    for (size_t i = 0; i < n; ++i) {
        int end = (i + 1 < n) ? boundary + step : width;
        f << '[' << boundary << '-' << end << "], ";
        boundary += step;
    }
    f << '\n';

    f << "Velocity avg (m/s), ";
    for (float v : sub_regions.avg())
        f << v << ", ";
    f << '\n';

    f << "Standard Deviation (m/s), ";
    for (float d : sub_regions.std_dev())
        f << d << ", ";
    f << '\n';

    f << "N. trajectories, ";
    for (float t : sub_regions.trajectories())
        f << t << ", ";
}

struct CliOptions
{
    std::filesystem::path video_path;
    std::filesystem::path config_path;
    std::filesystem::path mask_path;
    std::filesystem::path output_dir = "output";
    bool visualize = false;
};

static CliOptions
parse_cli(int argc, char** argv)
{
    auto options = CliOptions{};
    cxxopts::Options cli("otv-analyze", "perform OTV analysis on video files");

    cli.add_options()
        ("m,mask", "optional binary mask image (0 = exclude, 255 = include)", cxxopts::value<std::filesystem::path>())
        ("o,output", "output directory for analysis results", cxxopts::value<std::filesystem::path>()->default_value("output"))
        ("v,visualize", "write velocity visualization of tracked particles to output directory")
        ("h,help", "print help")
        ("video", "path to video file to analyze", cxxopts::value<std::filesystem::path>())
        ("config", "path to TOML configuration file", cxxopts::value<std::filesystem::path>());

    cli.parse_positional({"video", "config"});
    cli.positional_help("<video> <config>");

    try {
        auto result = cli.parse(argc, argv);

        if (result.count("help")) {
            fmt::println("{}", cli.help({""}));
            std::exit(0);
        }

        if (!result.count("video") || !result.count("config")) {
            fmt::println(stderr, "Error: <video> and <config> are required");
            fmt::println(stderr, "{}", cli.help({""}));
            std::exit(1);
        }

        options.video_path = result["video"].as<std::filesystem::path>();
        options.config_path = result["config"].as<std::filesystem::path>();

        if (result.count("mask")) {
            options.mask_path = result["mask"].as<std::filesystem::path>();
        }

        options.output_dir = result["output"].as<std::filesystem::path>();
        options.visualize = result["visualize"].as<bool>();

        return options;
    } catch (const std::exception& e) {
        fmt::println(stderr, "Error parsing arguments: {}", e.what());
        std::exit(1);
    }
}

static cv::Mat
fill_background(const cv::Mat& frame, const cv::Mat& colored, const cv::Mat& normalized)
{
    cv::Mat out(frame.size(), CV_8UC3, cv::Scalar(0, 0, 0));
    std::vector<cv::Mat> ch_out(3), ch_frame(3), ch_traj(3);
    cv::split(out, ch_out);
    cv::split(frame, ch_frame);
    cv::split(colored, ch_traj);

    for (int row = 0; row < frame.rows; ++row) {
        for (int col = 0; col < frame.cols; ++col) {
            if (normalized.ptr<uchar>(row)[col] == 0) {
                ch_out[0].ptr<uchar>(row)[col] = ch_frame[0].ptr<uchar>(row)[col];
                ch_out[1].ptr<uchar>(row)[col] = ch_frame[1].ptr<uchar>(row)[col];
                ch_out[2].ptr<uchar>(row)[col] = ch_frame[2].ptr<uchar>(row)[col];
            } else {
                ch_out[0].ptr<uchar>(row)[col] = ch_traj[0].ptr<uchar>(row)[col];
                ch_out[1].ptr<uchar>(row)[col] = ch_traj[1].ptr<uchar>(row)[col];
                ch_out[2].ptr<uchar>(row)[col] = ch_traj[2].ptr<uchar>(row)[col];
            }
        }
    }

    cv::merge(ch_out, out);
    return out;
}

static void
put_labels(const std::vector<std::string>& labels, cv::Mat& frame)
{
    int fontface = cv::FONT_HERSHEY_SIMPLEX;
    double scale = 0.5;
    int thickness = 1;

    for (int i = 0; i < static_cast<int>(labels.size()); ++i) {
        cv::Point pt(0, 20 + i * 20);
        int baseline = 0;
        cv::Size text = cv::getTextSize(labels[i], fontface, scale, thickness, &baseline);
        cv::rectangle(frame,
                      pt + cv::Point(0, baseline),
                      pt + cv::Point(text.width, -text.height),
                      cv::Scalar(0, 0, 0),
                      cv::FILLED);
        cv::putText(frame,
                    labels[i],
                    pt,
                    fontface,
                    scale,
                    cv::Scalar(255, 255, 255),
                    thickness,
                    cv::LINE_8);
    }
}

static void
write_video(const std::filesystem::path& video_path,
            const std::filesystem::path& output_dir,
            const otv_sdk::Config& config,
            const std::vector<otv_sdk::FrameSnapshot>& snapshots,
            const otv_sdk::VelocityStats& stats)
{
    cv::VideoCapture cap(video_path.string());
    if (!cap.isOpened()) {
        fmt::println(stderr, "Error: could not reopen video for visualization");
        return;
    }

    double fps = cap.get(cv::CAP_PROP_FPS);
    cv::Size target(config.video.width, config.video.height);
    std::filesystem::create_directories(output_dir);
    std::string out_path = (output_dir / "velocity.avi").string();

    // INVARIANT: Sentinel velocities anchor the colormap normalization range.
    float vel_max = (stats.count > 0) ? stats.max : 1.0f;
    float vel_min = (stats.count > 0) ? stats.min : 0.0f;

    cv::VideoWriter writer;
    cv::Mat vel_img;

    // INVARIANT: Running stats for the per-frame overlay (accumulated as trajectories appear).
    float running_sum = 0.0f, running_sum_sq = 0.0f;
    float running_max = 0.0f;
    float running_min = std::numeric_limits<float>::max();
    int running_count = 0;

    int total_snaps = static_cast<int>(snapshots.size());
    int snap_idx = 0;
    int raw_idx = 0;
    cv::Mat frame;

    while (cap.read(frame) && snap_idx < total_snaps) {
        if (raw_idx % config.video.jump != 0) {
            ++raw_idx;
            continue;
        }

        fmt::print("\r[{}/{}]", snap_idx + 1, total_snaps);
        std::fflush(stdout);

        if (frame.cols != target.width || frame.rows != target.height)
            cv::resize(frame, frame, target);

        if (snap_idx == 0) {
            vel_img = cv::Mat(frame.size(), CV_32F, cv::Scalar(0));
            writer.open(
                out_path, cv::VideoWriter::fourcc('M', 'J', 'P', 'G'), fps, frame.size(), true);
            if (!writer.isOpened()) {
                fmt::println(stderr, "Error: could not open '{}' for writing", out_path);
                return;
            }
        }

        // INVARIANT: Sentinel lines guarantee the normalization range spans [vel_min, vel_max].
        cv::line(vel_img, { 0, 0 }, { 0, 1 }, vel_max, 1, cv::LINE_AA);
        cv::line(vel_img, { 0, 1 }, { 0, 2 }, vel_min, 1, cv::LINE_AA);

        // INVARIANT: Draw this frame's validated trajectory segments onto the cumulative velocity
        // image.
        const auto& snap = snapshots[snap_idx];
        for (size_t j = 0; j < snap.current.size(); ++j) {
            cv::line(vel_img,
                     snap.current[j].pt,
                     snap.predicted[j].pt,
                     snap.velocities[j],
                     1,
                     cv::LINE_AA);

            float v = snap.velocities[j];
            ++running_count;
            running_sum += v;
            running_sum_sq += v * v;
            running_max = std::max(running_max, v);
            running_min = std::min(running_min, v);
        }

        cv::Mat normalized, colored;
        cv::normalize(vel_img, normalized, 0, 255, cv::NORM_MINMAX);
        normalized.convertTo(normalized, CV_8U);
        cv::applyColorMap(normalized, colored, cv::COLORMAP_JET);

        float avg_disp = (running_count > 0) ? running_sum / running_count : 0.0f;
        float variance = (running_count > 0)
                             ? std::max(0.0f, running_sum_sq / running_count - avg_disp * avg_disp)
                             : 0.0f;
        float std_disp = std::sqrt(variance);

        std::vector<std::string> labels = {
            running_count == 0 ? "velocity avg: none"
                               : fmt::format("velocity avg: {:.4f} m/s", avg_disp),
            running_count == 0 ? "velocity max: none"
                               : fmt::format("velocity max: {:.4f} m/s", running_max),
            running_count == 0 ? "velocity min: none"
                               : fmt::format("velocity min: {:.4f} m/s", running_min),
            running_count == 0 ? "standard deviation: none"
                               : fmt::format("standard deviation: {:.4f} m/s", std_disp),
            running_count == 0 ? "number of trajectories: none"
                               : fmt::format("number of trajectories: {}", running_count),
        };

        cv::Mat filled = fill_background(frame, colored, normalized);
        put_labels(labels, filled);
        writer << filled;

        ++snap_idx;
        ++raw_idx;
    }

    fmt::println("");
    fmt::println("Visualization written to '{}'", out_path);
}

static void
run_otv_engine(CliOptions options)
{
    otv_sdk::Config config = otv_sdk::Config::from_file(options.config_path);

    cv::VideoCapture cap(options.video_path.string());
    if (!cap.isOpened()) {
        fmt::println(stderr, "Error: could not open video '{}'", options.video_path.string());
        std::exit(1);
    }

    otv_sdk::OtvEngine engine(config);

    if (options.visualize)
        engine.enable_snapshots();

    if (!options.mask_path.empty()) {
        cv::Mat mask = cv::imread(options.mask_path.string(), cv::IMREAD_UNCHANGED);
        if (mask.empty()) {
            fmt::println(stderr, "Error: could not load mask '{}'", options.mask_path.string());
            std::exit(1);
        }
        engine.set_mask(mask);
    }

    // INVARIANT: Determine the number of frames to process.
    int total_raw = static_cast<int>(cap.get(cv::CAP_PROP_FRAME_COUNT));
    int total_processed = total_raw / config.video.jump;
    int raw_idx = 0;
    int processed = 0;
    cv::Size target(config.video.width, config.video.height);

    auto t_start = std::chrono::steady_clock::now();

    // INVARIANT: Process frame directly from video capture.
    //   - No need to save individual frames, just process them from capture!
    cv::Mat frame;
    while (cap.read(frame)) {
        if (raw_idx % config.video.jump == 0) {
            fmt::print("\r[{}/{}]", processed + 1, total_processed);
            std::fflush(stdout);

            // INVARIANT: Resize to the configured processing dimensions before tracking.
            if (frame.cols != target.width || frame.rows != target.height)
                cv::resize(frame, frame, target);

            engine.process_frame(frame);
            ++processed;
        }
        ++raw_idx;
    }
    fmt::println("");

    auto t_end = std::chrono::steady_clock::now();
    float elapsed =
        static_cast<float>(
            std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count()) /
        1'000'000.0f;

    auto stats = engine.stats();
    write_stats(options.output_dir, stats, elapsed);
    write_sub_regions(options.output_dir, engine.sub_regions(), config.video.width);

    if (options.visualize)
        write_video(
            options.video_path, options.output_dir, config, engine.snapshots(), stats);

    fmt::println("Done. Results written to '{}'", options.output_dir.string());
}

int
main(int argc, char** argv)
{
    auto options = parse_cli(argc, argv);
    try {
        run_otv_engine(options);
    } catch (const otv_sdk::ConfigError& error) {
        fmt::println(stderr, "Settings error: {}", error.what());
        return 1;
    } catch (const std::filesystem::filesystem_error& error) {
        fmt::println(stderr, "Filesystem error: {}", error.what());
        return 1;
    } catch(const std::exception& error) {
        fmt::println(stderr, "Failure: {}", error.what());
        return 1;
    }

    return 0;
}
