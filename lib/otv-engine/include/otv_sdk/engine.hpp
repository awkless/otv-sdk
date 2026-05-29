// SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
// SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OTV_ENGINE_HPP
#define OTV_ENGINE_HPP

#include <cstdint>
#include <filesystem>
#include <string_view>

#include <toml++/toml.hpp>

#include "otv_sdk/engine/export.hpp"

namespace otv_sdk {
/// @brief Feature detector algorithms.
enum class FeatureDetectorOption : uint8_t {
    FAST,
    ORB,
    SIFT,
    GFTT,
};

/// @brief OTV calibration parameters.
struct [[nodiscard]] CalibrationSettings {
    /// @brief Conversion factor from pixels to real-world units.
    float pixel_to_real;

    /// @brief Minimum angle boundary for partial (per-frame) motion.
    float partial_min_angle;

    /// @brief Maximum angle boundary for partial (per-frame) motion.
    float partial_max_angle;

    /// @brief Minimum angle boundary for trajectory filter.
    float final_min_angle;

    /// @brief Maximum angle boundary for trajectory filter.
    float final_max_angle;

    /// @brief Minimum displacement a tracked point must travel.
    float final_min_distance;

    /// @brief Maximum cap on the number of feature points to track.
    int max_features;

    /// @brief Processing scale: full frame (1.0), half-frame (0.5), quarter-frame (0.25).
    float resolution;

    /// @brief Spacing between sub-regions of the frame used for tacking.
    float region_step;

    /// @brief Feature detection algorithm to use.
    FeatureDetectorOption feature_detector;
};

/// @brief Lucas-Kanade configuration settings.
struct [[nodiscard]] LucasKanadeSettings {
    /// @brief Radius of search window.
    ///
    /// Search window width and height is `(2 * radius + 1)` in pixels.
    int radius;

    /// @brief The maximum number of pyramid levels.
    ///
    /// Higher values help track larger displacements but increase computation.
    int max_level;

    /// @brief The maximum number of iterations per pyramid level.
    int max_count;

    /// @brief The convergence threshold.
    ///
    /// Iteration stops when the solution is less than this value.
    double epsilon;

    /// @brief Minimum optical flow threshold.
    ///
    /// Points whose optical flow solution has a minimum eigen value below this
    /// threshold are rejected as poorly conditioned, i.e, textureless.
    double min_eig_threshold;
};

/// @brief Video configuration settings.
struct [[nodiscard]] VideoSettings {
    /// @brief The Frames Per Second (FPS) of the video.
    int fps;

    /// @brief The resolution width for a video frame.
    int width;

    /// @brief The resolution height for a video frame.
    int height;

    /// @brief The number of frames to skip after a processed frame.
    int jump;
};

/// @brief OTV configuration representation.
struct [[nodiscard]] Config {
    /// @brief Parse OTV configuration from file.
    ///
    /// @param[in] path Path to parse file.
    /// @return OTV configuration.
    ENGINE_EXPORT static Config from_file(std::filesystem::path& path);

    /// @brief Parse OTV configuration from string.
    ///
    /// @param[in] string String to parse.
    /// @return OTV configuration.
    ENGINE_EXPORT static Config from_str(std::string_view string);

    /// @brief Parsed calibration settings.
    CalibrationSettings calibration;

    /// @brief Parsed Lucas-Kanade settings.
    LucasKanadeSettings lucas_kanade;

    /// @brief Parsed video settings.
    VideoSettings video;
};

class ConfigError : public toml::parse_error {
public:
    using toml::parse_error::parse_error;
};
} // namespace otv

#endif // OTV_ENGINE_HPP
