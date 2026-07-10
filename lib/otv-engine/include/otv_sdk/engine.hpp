// SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
// SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef OTV_ENGINE_HPP
#define OTV_ENGINE_HPP

#include <cstdint>
#include <filesystem>
#include <string_view>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
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
    ENGINE_EXPORT static Config
    from_file(std::filesystem::path& path);

    /// @brief Parse OTV configuration from string.
    ///
    /// @param[in] string String to parse.
    /// @return OTV configuration.
    ENGINE_EXPORT static Config
    from_str(std::string_view string);

    /// @brief Parsed calibration settings.
    CalibrationSettings calibration;

    /// @brief Parsed Lucas-Kanade settings.
    LucasKanadeSettings lucas_kanade;

    /// @brief Parsed video settings.
    VideoSettings video;
};

struct KeyPoints {
    /// @brief Detected points from feature detector.
    std::vector<cv::KeyPoint> detected;

    /// @brief Current points being actively tracked.
    std::vector<cv::KeyPoint> current;

    /// @brief Points predicted by Lucas-Kanade for the next frame.
    std::vector<cv::KeyPoint> predicted;

    /// @brief Starting positions at the time each point was first detected.
    std::vector<cv::KeyPoint> start;

    /// @brief Frame index at which each tracked point was first detected.
    std::vector<int> timestamp;
};

/// @brief Aggregate velocity statistics across all tracked trajectories.
struct [[nodiscard]] VelocityStats {
    float avg = 0.0f;
    float max = 0.0f;
    float min = 0.0f;
    float std_dev = 0.0f;
    int count = 0;
};

/// @brief Per-frame record of validated trajectory segments for visualization.
///
/// Members current[i] and predicted[i] are the start and end of a single tracked segment, colored
/// by velocities[i]. Populated only when enable_snapshots() is called.
struct FrameSnapshot {
    std::vector<cv::KeyPoint> current;
    std::vector<cv::KeyPoint> predicted;
    std::vector<float> velocities;
};

/// @brief Automatic feature detection handler.
///
/// Selects and wraps the cv::Feature2D algorithm chosen by settings.
class FeatureDetector {
public:
    explicit FeatureDetector(const CalibrationSettings& calib, cv::Size frame_size);

    /// @brief Detect keypoints in image, writing results to keypoints.detected.
    void
    detect(cv::InputArray image, KeyPoints& keypoints, cv::InputArray mask = cv::noArray());

private:
    cv::Ptr<cv::Feature2D> m_detector;
};

/// @brief Lucas-Kanade optical flow tracker.
class LucasKanade {
public:
    explicit LucasKanade(const LucasKanadeSettings& lk);

    /// @brief Track keypoints between two frames.
    ///
    /// Converts current_kps to Point2f internally and writes results into m_lk_next.
    void
    track_points(cv::Mat previous_frame, cv::Mat current_frame,
        const std::vector<cv::KeyPoint>& current_kps);

    /// @brief Status vector from the last calcOpticalFlowPyrLK call.
    ///
    /// Each entry is 1 if the corresponding feature was found, 0 otherwise.
    [[nodiscard]]
    const std::vector<unsigned char>&
    status() const;

    /// @brief Output points from the last calcOpticalFlowPyrLK call.
    [[nodiscard]]
    const std::vector<cv::Point2f>&
    next_points() const;

private:
    LucasKanadeSettings m_lk_settings;
    std::vector<unsigned char> m_status;
    std::vector<float> m_err;
    std::vector<cv::Point2f> m_lk_prev;
    std::vector<cv::Point2f> m_lk_next;
    cv::Size m_min_size;
    cv::TermCriteria m_termcrit;
};

/// @brief Per-column sub-region velocity accumulator.
///
/// Divides the frame width into equal-width columns and accumulates per-column
/// velocity samples and trajectory counts.
class SubRegion {
public:
    explicit SubRegion(const CalibrationSettings& calib, int width);

    /// @brief Add new velocity to target sub-region.
    void
    add_velocity(size_t idx, float velocity);

    /// @brief Increment trajectory of target sub-region by one.
    void
    increment_trajectory(size_t idx);

    /// @brief Per-column average velocity.
    [[nodiscard]]
    std::vector<float>
    avg() const;

    /// @brief Per-column population standard deviation.
    [[nodiscard]]
    std::vector<float>
    std_dev() const;

    /// @brief Get trajectories of sub-regions.
    [[nodiscard]]
    const std::vector<int>&
    trajectories() const;

    /// @brief Pixel width of each sub-region column.
    [[nodiscard]]
    int
    step() const;

    /// @brief Size of sub-region.
    [[nodiscard]]
    size_t
    size() const;

private:
    int m_step;
    std::vector<std::vector<float>> m_velocities;
    std::vector<int> m_trajectories;
};

/// @brief Frame-by-frame OTV processing pipeline.
///
/// Accepts frames one at a time via process_frame(). Each call detects new
/// feature points, tracks existing ones with Lucas-Kanade, applies per-frame
/// and final trajectory filters, and accumulates velocities into sub-regions.
/// Call stats() and sub_regions() after processing all frames for results.
class OtvEngine {
public:
    explicit OtvEngine(const Config& config);

    /// @brief Register an optional binary mask image (0 = exclude, 255 = include).
    ///
    /// The mask is normalized to {0,1} and resized to the configured video
    /// dimensions before being applied to each subsequent frame.
    void
    set_mask(const cv::Mat& mask);

    /// @brief Process the next frame in sequence.
    ///
    /// Internally converts frame to grayscale (if needed), suppresses static
    /// background via a running average, and applies CLAHE contrast
    /// enhancement before feature detection and Lucas-Kanade tracking.
    void
    process_frame(const cv::Mat& frame);

    /// @brief Corrected frame (background-subtracted + CLAHE) used for tracking in the
    /// most recent process_frame() call. Intended as the display background for live
    /// visualization, since the raw input frame is often too dark/low-contrast on its own.
    [[nodiscard]]
    const cv::Mat&
    corrected_frame() const;

    /// @brief Enable per-frame snapshot recording for visualization output.
    ///
    /// Must be called before any process_frame() calls. When enabled,
    /// snapshots() returns per-frame data populated with validated trajectory
    /// segments — suitable for driving write_video() in a caller.
    void
    enable_snapshots();

    /// @brief Overall velocity statistics across all processed frames.
    [[nodiscard]]
    VelocityStats
    stats() const;

    /// @brief Get current sub-region.
    [[nodiscard]]
    const SubRegion&
    sub_regions() const;

    /// @brief Per-frame snapshots (empty unless enable_snapshots() was called).
    [[nodiscard]]
    const std::vector<FrameSnapshot>&
    snapshots() const;

private:
    void
    apply_mask(cv::Mat& frame) const;
    void
    admit_new_points();
    void
    determine_optical_flow(const cv::Mat& current);
    void
    track_trajectory(size_t idx, float scaled_min_dist, float scaled_pixel_to_real);

    struct HistoryEntry {
        int frame_idx = 0;
        cv::KeyPoint current = {};
        cv::KeyPoint predicted = {};
    };

    Config m_config;
    float m_effective_fps;
    FeatureDetector m_detector;
    LucasKanade m_lk;
    SubRegion m_sub_regions;
    KeyPoints m_keypoints;
    cv::Mat m_mask;
    cv::Mat m_background;
    cv::Mat m_previous_frame;
    std::vector<std::vector<float>> m_frame_velocities;
    int m_frame_idx;
    bool m_record_snapshots;
    std::vector<FrameSnapshot> m_snapshots;
    std::vector<std::vector<HistoryEntry>> m_point_histories;
};

class ConfigError : public toml::parse_error {
public:
    using toml::parse_error::parse_error;
};
} // namespace otv

#endif // OTV_ENGINE_HPP
