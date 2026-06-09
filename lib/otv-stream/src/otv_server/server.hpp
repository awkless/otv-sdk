/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef OTV_STREAM_SERVER_HPP
#define OTV_STREAM_SERVER_HPP

#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <vector>
#include <sys/mman.h>

#include <libcamera/libcamera.h>
#include <opencv2/imgproc.hpp>
#include <spdlog/spdlog.h>

#include <otv_sdk/engine.hpp>

struct RequestQueue {
    std::queue<libcamera::Request*> pending;
    std::mutex pending_mutex;
    std::condition_variable pending_cv;

    void on_request_completed(libcamera::Request* request);
    libcamera::Request* dequeue_request();
};

class Rpi4BCamera final {
public:
    Rpi4BCamera(const otv_sdk::Config& config, RequestQueue& queue);
    ~Rpi4BCamera() noexcept;

    void start();
    void stop();
    const libcamera::StreamConfiguration& stream_cfg() const;
    libcamera::Stream* stream() const;
    libcamera::Request* dequeue_request();
    std::shared_ptr<libcamera::Camera> camera();

private:
    void open_and_acquire();
    void configure_stream(const otv_sdk::Config &config);
    void setup_request_capture();

    std::unique_ptr<libcamera::CameraManager> m_cam_manager;
    std::shared_ptr<libcamera::Camera> m_camera;
    std::unique_ptr<libcamera::CameraConfiguration> m_cam_config;
    libcamera::StreamConfiguration m_stream_cfg;
    libcamera::Stream* m_stream;
    std::unique_ptr<libcamera::FrameBufferAllocator> m_allocator;
    std::vector<std::unique_ptr<libcamera::Request>> m_requests;
    RequestQueue& m_queue;
};

class OtvAnalyzer final {
public:
    OtvAnalyzer(const otv_sdk::Config& config, Rpi4BCamera& rpi_cam, std::atomic<bool>& stop);

    void run();
    void log_results();

private:
    cv::Mat buffer_to_gray(const libcamera::FrameBuffer* buffer) const;
    cv::Mat scale_frame(const cv::Mat& src) const;

    otv_sdk::OtvEngine m_engine;
    const otv_sdk::Config& m_config;
    Rpi4BCamera& m_rpi_cam;
    std::atomic<bool>& m_stop;
    int m_fed_frame;
};

class Rpi4BCameraError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

#endif /* OTV_STREAM_SERVER_HPP */
