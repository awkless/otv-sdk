// SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
// SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
// SPDX-License-Identifier: GPL-3.0-or-later

#include <filesystem>
#include <optional>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <limits>

#include <opencv2/core.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/video/tracking.hpp>
#include <fmt/format.h>
#include <toml++/toml.hpp>

#include "otv_sdk/engine.hpp"

namespace otv_sdk {
namespace {
constexpr float FULL_FRAME_RESOLUTION = 1.0f;
constexpr float HALF_FRAME_RESOLUTION = 0.5f;
constexpr float QUARTER_FRAME_RESOLUTION = 0.25f;
constexpr float PI = 3.14159265358979323846f;

float
get_angle(const cv::KeyPoint& kp1, const cv::KeyPoint& kp2)
{
    // INVARIANT: Angle in degrees of the displacement vector from kp1 to kp2.
    constexpr float k_rad_to_degree = 180.0f / PI;
    return std::atan2(kp2.pt.y - kp1.pt.y, kp2.pt.x - kp1.pt.x) * k_rad_to_degree;
}

float
get_velocity(const cv::KeyPoint& kp1,
             const cv::KeyPoint& kp2,
             float pixel_to_real,
             float time_delta,
             float fps)
{
    // INVARIANT: Real-world velocity for a tracked trajectory.
    float magnitude = static_cast<float>(cv::norm(kp2.pt - kp1.pt));
    return magnitude * pixel_to_real * fps / time_delta;
}

bool
angle_in_range(const cv::KeyPoint& kp1, const cv::KeyPoint& kp2, float min_angle, float max_angle)
{
    float angle = get_angle(kp1, kp2);
    return angle > min_angle && angle < max_angle;
}

bool
partial_filter(const cv::KeyPoint& kp1,
               const cv::KeyPoint& kp2,
               float min_angle,
               float max_angle,
               float max_distance)
{
    // INVARIANT: Accept a point if its displacement is within the expected angle range and does
    // not exceed max_distance pixels.
    if (static_cast<float>(cv::norm(kp2.pt - kp1.pt)) > max_distance)
        return false;
    return angle_in_range(kp1, kp2, min_angle, max_angle);
}

bool
final_filter(const cv::KeyPoint& kp1,
             const cv::KeyPoint& kp2,
             float min_distance,
             float min_angle,
             float max_angle)
{
    // INVARIANT: Accept a completed trajectory if it has traveled at least min_distance pixels
    // within the expected angle range.
    if (static_cast<float>(cv::norm(kp2.pt - kp1.pt)) < min_distance)
        return false;
    return angle_in_range(kp1, kp2, min_angle, max_angle);
}

VelocityStats
compute_stats(const std::vector<std::vector<float>>& velocities)
{
    // INVARIANT: Only positive velocities contribute to statistics.
    //   - Compute velocity statistics over all frames.
    VelocityStats result;
    result.min = std::numeric_limits<float>::max();

    for (const auto& frame : velocities) {
        for (float vel : frame) {
            if (vel > 0.0f) {
                result.avg += vel;
                result.max = std::max(result.max, vel);
                result.min = std::min(result.min, vel);
                ++result.count;
            }
        }
    }

    if (result.count == 0) {
        result.min = 0.0f;
        return result;
    }

    result.avg /= static_cast<float>(result.count);

    float variance = 0.0f;
    for (const auto& frame : velocities) {
        for (float vel : frame) {
            if (vel > 0.0f) {
                float diff = vel - result.avg;
                variance += diff * diff;
            }
        }
    }
    result.std_dev = std::sqrt(variance / static_cast<float>(result.count));

    return result;
}
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

FeatureDetector::FeatureDetector(const CalibrationSettings& calib, cv::Size frame_size)
{
    int n_features = frame_size.width * frame_size.height;

    switch (calib.feature_detector) {
        case FeatureDetectorOption::FAST:
            m_detector = cv::FastFeatureDetector::create();
            break;
        case FeatureDetectorOption::ORB:
            m_detector = cv::ORB::create(n_features);
            break;
        case FeatureDetectorOption::SIFT:
            m_detector = cv::SIFT::create(n_features);
            break;
        case FeatureDetectorOption::GFTT:
            m_detector = cv::GFTTDetector::create(n_features);
            break;
        default:
            m_detector = cv::FastFeatureDetector::create();
            break;
    }
}

void
FeatureDetector::detect(cv::InputArray image, KeyPoints& keypoints, cv::InputArray mask)
{
    m_detector->detect(image, keypoints.detected, mask);
}

LucasKanade::LucasKanade(const LucasKanadeSettings& lk)
    : m_lk_settings(lk)
{
    m_min_size = cv::Size(lk.radius * 2 + 1, lk.radius * 2 + 1);
    m_termcrit =
        cv::TermCriteria(cv::TermCriteria::COUNT | cv::TermCriteria::EPS, lk.max_count, lk.epsilon);
}

void
LucasKanade::track_points(cv::Mat previous_frame,
                          cv::Mat current_frame,
                          const std::vector<cv::KeyPoint>& current_kps)
{
    cv::KeyPoint::convert(current_kps, m_lk_prev);
    cv::calcOpticalFlowPyrLK(previous_frame,
                             current_frame,
                             m_lk_prev,
                             m_lk_next,
                             m_status,
                             m_err,
                             m_min_size,
                             m_lk_settings.max_level,
                             m_termcrit,
                             cv::OPTFLOW_LK_GET_MIN_EIGENVALS,
                             m_lk_settings.min_eig_threshold);
}

const std::vector<unsigned char>&
LucasKanade::status() const
{
    return m_status;
}

const std::vector<cv::Point2f>&
LucasKanade::next_points() const
{
    return m_lk_next;
}

SubRegion::SubRegion(const CalibrationSettings& calib, int width)
    : m_step(static_cast<int>(calib.region_step * calib.resolution))
{
    int n_regions = static_cast<int>(std::ceil(static_cast<float>(width) / static_cast<float>(m_step)));
    m_velocities.resize(n_regions);
    m_trajectories.resize(n_regions, 0);
}

void
SubRegion::add_velocity(size_t idx, float velocity)
{
    m_velocities[idx].push_back(velocity);
}

void
SubRegion::increment_trajectory(size_t idx)
{
    m_trajectories[idx]++;
}

std::vector<float>
SubRegion::avg() const
{
    std::vector<float> averages(m_velocities.size(), 0.0f);

    for (size_t i = 0; i < m_velocities.size(); ++i) {
        if (m_velocities[i].empty())
            continue;
        float sum = 0.0f;
        for (float v : m_velocities[i])
            sum += v;
        averages[i] = sum / static_cast<float>(m_velocities[i].size());
    }

    return averages;
}

std::vector<float>
SubRegion::std_dev() const
{
    std::vector<float> averages = avg();
    std::vector<float> devs(m_velocities.size(), 0.0f);

    for (size_t i = 0; i < m_velocities.size(); ++i) {
        if (m_velocities[i].empty())
            continue;
        float sum_sq = 0.0f;
        for (float v : m_velocities[i]) {
            float diff = v - averages[i];
            sum_sq += diff * diff;
        }
        devs[i] = std::sqrt(sum_sq / static_cast<float>(m_velocities[i].size()));
    }

    return devs;
}

const std::vector<int>&
SubRegion::trajectories() const
{
    return m_trajectories;
}

int
SubRegion::step() const
{
    return m_step;
}

size_t
SubRegion::size() const
{
    return m_velocities.size();
}

OtvEngine::OtvEngine(const Config& config)
    : m_config(config)
    , m_effective_fps(static_cast<float>(config.video.fps) / static_cast<float>(config.video.jump))
    , m_detector(config.calibration, cv::Size(config.video.width, config.video.height))
    , m_lk(config.lucas_kanade)
    , m_sub_regions(config.calibration, config.video.width)
    , m_frame_idx(0)
    , m_record_snapshots(false)
{
}

void
OtvEngine::enable_snapshots()
{
    m_record_snapshots = true;
}

const std::vector<FrameSnapshot>&
OtvEngine::snapshots() const
{
    return m_snapshots;
}

void
OtvEngine::set_mask(const cv::Mat& mask)
{
    constexpr int bit_value = 255;

    if (mask.channels() > 1)
        cv::cvtColor(mask, m_mask, cv::COLOR_BGR2GRAY);
    else
        m_mask = mask;

    m_mask = m_mask / bit_value;
    cv::Size target(m_config.video.width, m_config.video.height);

    if (m_mask.size() != target)
        cv::resize(m_mask, m_mask, target);
}

void
OtvEngine::apply_mask(cv::Mat& frame) const
{
    std::vector<cv::Mat> channels;
    cv::split(frame, channels);

    for (auto& ch : channels)
        ch = ch.mul(m_mask);

    cv::merge(channels, frame);
}

void
OtvEngine::admit_new_points()
{
    // INVARIANT: Admit newly detected points up to the max_features budget.
    int budget = m_config.calibration.max_features - static_cast<int>(m_keypoints.current.size());
    if (budget > 0) {
        int n_detected = static_cast<int>(m_keypoints.detected.size());
        int n_add = std::min(budget, n_detected);
        for (int i = n_detected - n_add; i < n_detected; ++i) {
            m_keypoints.current.push_back(m_keypoints.detected[i]);
            m_keypoints.start.push_back(m_keypoints.detected[i]);
            m_keypoints.timestamp.push_back(m_frame_idx);

            if (m_record_snapshots)
                m_point_histories.emplace_back();
        }
    }

    m_frame_velocities.emplace_back();
}

void
OtvEngine::determine_optical_flow(const cv::Mat& current)
{
    m_lk.track_points(m_previous_frame, current, m_keypoints.current);

    m_keypoints.predicted.clear();
    for (const auto& pt : m_lk.next_points())
        m_keypoints.predicted.emplace_back(pt, 1.0f);
}

void
OtvEngine::track_trajectory(size_t idx, float scaled_min_dist, float scaled_pixel_to_real)
{
    // INVARIANT: Point has left tracking, check if the full trajectory is valid.
    if (final_filter(m_keypoints.start[idx],
                     m_keypoints.current[idx],
                     scaled_min_dist,
                     m_config.calibration.final_min_angle,
                     m_config.calibration.final_max_angle)) {
        auto dt = static_cast<float>(m_frame_idx - m_keypoints.timestamp[idx]);
        float vel = get_velocity(m_keypoints.start[idx],
                                 m_keypoints.current[idx],
                                 scaled_pixel_to_real,
                                 dt,
                                 m_effective_fps);

        int region_start = static_cast<int>(m_keypoints.start[idx].pt.x) / m_sub_regions.step();
        int region_cur = static_cast<int>(m_keypoints.current[idx].pt.x) / m_sub_regions.step();

        if (region_start == region_cur) {
            m_sub_regions.add_velocity(static_cast<size_t>(region_start), vel);
            m_sub_regions.increment_trajectory(static_cast<size_t>(region_start));
        }

        m_frame_velocities.back().push_back(vel);

        // INVARIANT: Retroactively mark all recorded segments of this trajectory with
        // the finalized velocity so the visualization shows the full path in color.
        if (m_record_snapshots) {
            for (const auto& entry : m_point_histories[idx]) {
                m_snapshots[entry.frame_idx].current.push_back(entry.current);
                m_snapshots[entry.frame_idx].predicted.push_back(entry.predicted);
                m_snapshots[entry.frame_idx].velocities.push_back(vel);
            }
        }
    }
}

void
OtvEngine::process_frame(const cv::Mat& frame)
{
    cv::Mat current = frame.clone();

    // Detect on the unmodified frame using the mask natively so the hard
    // zero-boundary does not create spurious corners for the feature detector.
    m_detector.detect(current, m_keypoints, m_mask);
    admit_new_points();

    // Zero pixels outside the mask after detection so LK optical flow cannot
    // follow gradients that cross the mask boundary.
    if (!m_mask.empty())
        apply_mask(current);

    if (m_frame_idx > 0 && !m_keypoints.current.empty()) {
        determine_optical_flow(current);
        const auto& lk_status = m_lk.status();

        // INVARIANT: Search distance scales inversely with resolution.
        //   - At half resolution, pixel displacements are halved so the window must be doubled.
        float max_partial_dist =
            static_cast<float>(m_config.lucas_kanade.max_level * (2 * m_config.lucas_kanade.radius + 1)) /
            m_config.calibration.resolution;
        float scaled_min_dist = m_config.calibration.final_min_distance * m_config.calibration.resolution;
        float scaled_pixel_to_real = m_config.calibration.pixel_to_real / m_config.calibration.resolution;

        if (m_record_snapshots)
            m_snapshots.resize(m_frame_idx + 1);

        size_t k = 0;
        for (size_t i = 0; i < m_keypoints.current.size(); ++i) {
            bool tracked = lk_status[i] && partial_filter(m_keypoints.current[i],
                                                          m_keypoints.predicted[i],
                                                          m_config.calibration.partial_min_angle,
                                                          m_config.calibration.partial_max_angle,
                                                          max_partial_dist);

            if (!tracked) {
                track_trajectory(i, scaled_min_dist, scaled_pixel_to_real);
                continue;
            }

            // INVARIANT: preserve only successfully tracked points.
            if (m_record_snapshots) {
                m_point_histories[i].push_back(
                    { m_frame_idx, m_keypoints.current[i], m_keypoints.predicted[i] });

                if (k != i) {
                    m_point_histories[k] = std::move(m_point_histories[i]);
                }
            }

            m_keypoints.current[k] = m_keypoints.current[i];
            m_keypoints.start[k] = m_keypoints.start[i];
            m_keypoints.predicted[k] = m_keypoints.predicted[i];
            m_keypoints.timestamp[k] = m_keypoints.timestamp[i];
            ++k;
        }

        m_keypoints.current.resize(k);
        m_keypoints.start.resize(k);
        m_keypoints.predicted.resize(k);
        m_keypoints.timestamp.resize(k);

        if (m_record_snapshots)
            m_point_histories.resize(k);
    }

    current.copyTo(m_previous_frame);

    if (!m_keypoints.predicted.empty())
        std::swap(m_keypoints.current, m_keypoints.predicted);

    ++m_frame_idx;
}

VelocityStats
OtvEngine::stats() const
{
    return compute_stats(m_frame_velocities);
}

const SubRegion&
OtvEngine::sub_regions() const
{
    return m_sub_regions;
}
} // namespace otv
