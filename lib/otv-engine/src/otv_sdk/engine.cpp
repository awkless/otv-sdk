// SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
// SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <optional>
#include <string_view>

#include <fmt/format.h>
#include <toml++/toml.hpp>

#include "otv_sdk/engine.hpp"

namespace otv_sdk {
namespace {
constexpr float FULL_FRAME_RESOLUTION = 1.0f;
constexpr float HALF_FRAME_RESOLUTION = 0.5f;
constexpr float QUARTER_FRAME_RESOLUTION = 0.25f;
} // anonymous namespace

template <typename T, typename E>
[[nodiscard]] T
value_required(const std::optional<T>& option, const E& error)
{
    if (!option) {
        throw error;
    }

    return *option;
}

[[nodiscard]] static CalibrationSettings
parse_calibration_settings(const toml::table& toml)
{
    auto calib = CalibrationSettings {};
    calib.pixel_to_real = value_required(
        toml["calibration"]["pixel_to_real"].value<float>(),
        ConfigError("missing float value for calibration.pixel_to_real", toml.source()));
    calib.partial_min_angle = value_required(
        toml["calibration"]["partial_min_angle"].value<float>(),
        ConfigError("missing float value for calibration.partial_min_angle", toml.source()));
    calib.partial_max_angle = value_required(
        toml["calibration"]["partial_max_angle"].value<float>(),
        ConfigError("missing float value for calibration.partial_max_angle", toml.source()));
    calib.final_min_angle = value_required(
        toml["calibration"]["final_min_angle"].value<float>(),
        ConfigError("missing float value for calibration.final_min_angle", toml.source()));
    calib.final_max_angle = value_required(
        toml["calibration"]["final_max_angle"].value<float>(),
        ConfigError("missing float value for calibration.final_max_angle", toml.source()));
    calib.final_min_distance = value_required(
        toml["calibration"]["final_min_distance"].value<float>(),
        ConfigError("missing float value for calibration.final_min_distance", toml.source()));
    calib.max_features = value_required(
        toml["calibration"]["max_features"].value<int>(),
        ConfigError("missing int value for calibration.max_features", toml.source()));
    calib.region_step = value_required(
        toml["calibration"]["region_step"].value<float>(),
        ConfigError("missing float value for calibration.region_step", toml.source()));

    auto resopt = value_required(
        toml["calibration"]["resolution"].value<std::string>(),
        ConfigError("missing string value from calibration.resolution", toml.source()));
    if (resopt == "full_frame") {
        calib.resolution = FULL_FRAME_RESOLUTION;
    } else if (resopt == "half_frame") {
        calib.resolution = HALF_FRAME_RESOLUTION;
    } else if (resopt == "quarter_frame") {
        calib.resolution = QUARTER_FRAME_RESOLUTION;
    } else {
        throw ConfigError(
            fmt::format("calibration.resolution set to unknown option '{}'", resopt).c_str(),
            toml.source());
    }

    auto detector = value_required(
        toml["calibration"]["feature_detector"].value<std::string>(),
        ConfigError("missing string value from calibration.feature_detector", toml.source()));
    if (detector == "FAST") {
        calib.feature_detector = FeatureDetectorOption::FAST;
    } else if (detector == "ORB") {
        calib.feature_detector = FeatureDetectorOption::ORB;
    } else if (detector == "SIFT") {
        calib.feature_detector = FeatureDetectorOption::SIFT;
    } else if (detector == "GFTT") {
        calib.feature_detector = FeatureDetectorOption::GFTT;
    } else {
        throw ConfigError(
            fmt::format("calibration.feature_detector set to unknown option '{}'", detector)
                .c_str(),
            toml.source());
    }

    return calib;
}

[[nodiscard]] static LucasKanadeSettings
parse_lucas_kanade_settings(const toml::table& toml)
{
    auto lk = LucasKanadeSettings {};
    lk.radius = value_required(toml["lukas_kanade"]["radius"].value<int>(),
        ConfigError("missing int value for lukas_kanade.radius", toml.source()));
    lk.max_level = value_required(
        toml["lukas_kanade"]["max_level"].value<int>(),
        ConfigError("missing int value for lukas_kanade.max_level", toml.source()));
    lk.max_count = value_required(
        toml["lukas_kanade"]["max_count"].value<int>(),
        ConfigError("missing int value for lukas_kanade.max_count", toml.source()));
    lk.epsilon = value_required(
        toml["lukas_kanade"]["epsilon"].value<double>(),
        ConfigError("missing double value for lukas_kanade.epsilon", toml.source()));
    lk.min_eig_threshold = value_required(
        toml["lukas_kanade"]["min_eig_threshold"].value<double>(),
        ConfigError("missing double value for lukas_kanade.min_eig_threshold", toml.source()));

    return lk;
}

[[nodiscard]] static VideoSettings
parse_video_settings(const toml::table& toml)
{
    auto video = VideoSettings {};
    video.fps = value_required(toml["video_info"]["fps"].value<int>(),
        ConfigError("missing int value for video_info.fps", toml.source()));
    video.width = value_required(toml["video_info"]["width"].value<int>(),
        ConfigError("missing int value for video_info.width", toml.source()));
    video.height = value_required(toml["video_info"]["height"].value<int>(),
        ConfigError("missing int value for video_info.height", toml.source()));
    video.jump = value_required(toml["video_info"]["jump"].value<int>(),
        ConfigError("missing int value for video_info.jump", toml.source()));

    return video;
}

Config Config::from_file(std::filesystem::path& path)
{
    auto config = Config {};

    try {
        toml::table toml = toml::parse_file(std::string_view { path.string() });
        config.calibration = parse_calibration_settings(toml);
        config.lucas_kanade = parse_lucas_kanade_settings(toml);
        config.video = parse_video_settings(toml);
    } catch (const toml::parse_error& error) {
        throw ConfigError(error.description().data(), std::move(error.source()));
    }

    return config;
}

Config Config::from_str(std::string_view string)
{
    auto config = Config {};

    try {
        toml::table toml = toml::parse(string);
        config.calibration = parse_calibration_settings(toml);
        config.lucas_kanade = parse_lucas_kanade_settings(toml);
        config.video = parse_video_settings(toml);
    } catch (const toml::parse_error& error) {
        throw ConfigError(error.description().data(), std::move(error.source()));
    }

    return config;
}
} // namespace otv
