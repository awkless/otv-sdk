/* SPDX-FileCopyrightText: 2026 Jason Pena <jasonpena@awkless.com>
 * SPDX-FileCopyrightText: 2026 Huidae Cho <hcho@nmsu.edu>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "server.hpp"

void RequestQueue::on_request_completed(libcamera::Request* request)
{
    if (request->status() == libcamera::Request::RequestCancelled)
        return;

    {
        std::lock_guard lock(pending_mutex);
        pending.push({ request });
    }

    pending_cv.notify_one();
}

libcamera::Request* RequestQueue::dequeue_request()
{
    std::unique_lock lock(pending_mutex);
    pending_cv.wait_for(lock, std::chrono::milliseconds(500), [this] { return !pending.empty(); });

    if (pending.empty()) {
        return nullptr;
    }

    libcamera::Request* request = pending.front();
    pending.pop();

    return request;
}

Rpi4BCamera::Rpi4BCamera(const otv_sdk::Config& config, RequestQueue& queue)
    : m_queue(queue)
{
    open_and_acquire();
    configure_stream(config);
    setup_request_capture();
}

Rpi4BCamera::~Rpi4BCamera()
{
    m_camera->stop();
    m_camera->requestCompleted.disconnect(this);
    m_allocator->free(m_stream);
    m_allocator.reset();
    m_requests.clear();
    m_camera->release();
    m_cam_manager->stop();
}

void Rpi4BCamera::open_and_acquire()
{
    m_cam_manager = std::make_unique<libcamera::CameraManager>();
    if (m_cam_manager->start()) {
        throw Rpi4BCameraError("failed to start camera manager");
    }

    auto cameras = m_cam_manager->cameras();
    if (cameras.empty()) {
        throw Rpi4BCameraError("no cameras found");
    }
    spdlog::info("found {} camera(s)", cameras.size());

    m_camera = m_cam_manager->get(cameras[0]->id());
    if (!m_camera) {
        throw Rpi4BCameraError("failed to get camera handle");
    }

    if (m_camera->acquire()) {
        throw Rpi4BCameraError("failed to acquire camera");
    }
    spdlog::info("acquired camera: {}", m_camera->id());
}

void Rpi4BCamera::configure_stream(const otv_sdk::Config& config)
{
    m_cam_config = m_camera->generateConfiguration({ libcamera::StreamRole::Viewfinder });
    if (!m_cam_config) {
        m_camera->release();
        throw Rpi4BCameraError("failed to generate camera configuration");
    }

    libcamera::StreamConfiguration& stream_cfg = m_cam_config->at(0);
    stream_cfg.size.width = static_cast<unsigned int>(config.video.width);
    stream_cfg.size.height = static_cast<unsigned int>(config.video.height);
    stream_cfg.pixelFormat = libcamera::formats::YUV420;
    stream_cfg.bufferCount = 4;

    switch (m_cam_config->validate()) {
    case libcamera::CameraConfiguration::Valid:
        break;
    case libcamera::CameraConfiguration::Adjusted:
        spdlog::warn("camera configuration was adjusted by libcamera - "
                     "stream is {}x{} fmt={}",
            stream_cfg.size.width, stream_cfg.size.height, stream_cfg.pixelFormat.toString());
        break;
    case libcamera::CameraConfiguration::Invalid:
        m_camera->release();
        throw Rpi4BCameraError("camera configuration is invalid");
    }

    if (m_camera->configure(m_cam_config.get())) {
        m_camera->release();
        throw Rpi4BCameraError("failed to configure camera");
    }

    m_stream_cfg = stream_cfg;
    m_stream = stream_cfg.stream();

    spdlog::info("camera configured: {}x{} @ {} fps (fmt={})", m_stream_cfg.size.width,
        m_stream_cfg.size.height, config.video.fps, m_stream_cfg.pixelFormat.toString());
}

void Rpi4BCamera::setup_request_capture()
{
    m_allocator = std::make_unique<libcamera::FrameBufferAllocator>(m_camera);
    if (m_allocator->allocate(m_stream) < 0) {
        m_camera->release();
        throw Rpi4BCameraError("failed to allocate frame buffers");
    }

    const auto& buffers = m_allocator->buffers(m_stream);
    spdlog::info("allocated {} frame buffers", buffers.size());

    m_requests.reserve(buffers.size());
    for (const auto& buf : buffers) {
        auto req = m_camera->createRequest();
        if (!req) {
            m_camera->release();
            throw Rpi4BCameraError("failed to create capture request");
        }

        if (req->addBuffer(m_stream, buf.get())) {
            m_camera->release();
            throw Rpi4BCameraError("failed to attach buffer to request");
        }

        m_requests.push_back(std::move(req));
    }

    m_camera->requestCompleted.connect(
        this, [this](libcamera::Request* req) { m_queue.on_request_completed(req); });
}

void Rpi4BCamera::start()
{
    if (m_camera->start()) {
        throw Rpi4BCameraError("failed to start camera");
    }

    for (auto& req : m_requests) {
        if (m_camera->queueRequest(req.get())) {
            m_camera->stop();
            m_camera->release();
            throw Rpi4BCameraError("failed to queue initial capture request");
        }
    }

    spdlog::info("camera capture started");
}

void Rpi4BCamera::stop()
{
    spdlog::info("camera capture stopped");
    m_camera->stop();
}

const libcamera::StreamConfiguration& Rpi4BCamera::stream_cfg() const { return m_stream_cfg; }

libcamera::Stream* Rpi4BCamera::stream() const { return m_stream; }

libcamera::Request* Rpi4BCamera::dequeue_request() { return m_queue.dequeue_request(); }

std::shared_ptr<libcamera::Camera> Rpi4BCamera::camera() { return m_camera; }

OtvAnalyzer::OtvAnalyzer(
    const otv_sdk::Config& config, Rpi4BCamera& rpi_cam, std::atomic<bool>& stop)
    : m_engine(otv_sdk::OtvEngine(config))
    , m_config(config)
    , m_rpi_cam(rpi_cam)
    , m_stop(stop)
    , m_fed_frame(0)
{
}

void OtvAnalyzer::run()
{
    const int jump = m_config.video.jump;
    int raw_frame = 0;
    int fed_frame = 0;

    m_rpi_cam.start();
    while (!m_stop.load()) {
        libcamera::Request* request = m_rpi_cam.dequeue_request();
        if (!request)
            continue;

        const bool should_process = (raw_frame % jump == 0);
        ++raw_frame;

        if (should_process) {
            const libcamera::FrameBuffer* fb = request->buffers().at(m_rpi_cam.stream());

            cv::Mat gray = buffer_to_gray(fb);
            if (gray.empty()) {
                spdlog::warn("Frame {} produced an empty Mat — skipping", raw_frame - 1);
            } else {
                cv::Mat scaled = scale_frame(gray);
                m_engine.process_frame(scaled);
                ++fed_frame;

                if (fed_frame % 30 == 0) {
                    const otv_sdk::VelocityStats s = m_engine.stats();
                    spdlog::info("Frames processed: {}  |  velocity — "
                                 "avg={:.3f}  max={:.3f}  min={:.3f}  "
                                 "stddev={:.3f}  n={}",
                        fed_frame, s.avg, s.max, s.min, s.std_dev, s.count);
                }
            }
        }

        request->reuse(libcamera::Request::ReuseBuffers);
        if (m_rpi_cam.camera()->queueRequest(request)) {
            spdlog::error("Failed to requeue request — stopping");
            m_stop.store(true);
        }
    }

    m_rpi_cam.stop();
    m_fed_frame = fed_frame;
}

void OtvAnalyzer::log_results()
{
    const otv_sdk::VelocityStats s = m_engine.stats();
    spdlog::info("--- Final velocity statistics ({} frames processed) ---", m_fed_frame);
    spdlog::info("  avg    = {:.4f}", s.avg);
    spdlog::info("  max    = {:.4f}", s.max);
    spdlog::info("  min    = {:.4f}", s.min);
    spdlog::info("  stddev = {:.4f}", s.std_dev);
    spdlog::info("  count  = {}", s.count);

    const otv_sdk::SubRegion& sub = m_engine.sub_regions();
    const auto avg_col = sub.avg();
    const auto& traj_col = sub.trajectories();
    spdlog::info("--- Sub-region summary ({} columns, step={} px) ---", sub.size(), sub.step());
    for (std::size_t col = 0; col < sub.size(); ++col) {
        spdlog::info(
            "  col {:3d}: avg_vel={:.4f}  trajectories={}", col, avg_col[col], traj_col[col]);
    }
}

cv::Mat OtvAnalyzer::buffer_to_gray(const libcamera::FrameBuffer* buffer) const
{
    const auto& planes = buffer->planes();
    if (planes.empty())
        return {};

    const libcamera::FrameBuffer::Plane& plane0 = planes[0];
    const std::size_t length = plane0.length;

    void* mapped = ::mmap(nullptr, length, PROT_READ, MAP_SHARED, plane0.fd.get(), plane0.offset);
    if (mapped == MAP_FAILED) {
        spdlog::error("mmap failed: {}", std::strerror(errno));
        return {};
    }

    const int width = static_cast<int>(m_rpi_cam.stream_cfg().size.width);
    const int height = static_cast<int>(m_rpi_cam.stream_cfg().size.height);
    const auto pixel_fmt = m_rpi_cam.stream_cfg().pixelFormat;
    cv::Mat gray;

    if (pixel_fmt == libcamera::formats::YUYV) {
        cv::Mat yuyv(height, width, CV_8UC2, mapped,
            static_cast<std::size_t>(m_rpi_cam.stream_cfg().stride));
        cv::cvtColor(yuyv, gray, cv::COLOR_YUV2GRAY_YUYV);
    } else if (pixel_fmt == libcamera::formats::NV12) {
        cv::Mat nv12(height + height / 2, width, CV_8UC1, mapped);
        cv::cvtColor(nv12, gray, cv::COLOR_YUV2GRAY_NV12);
    } else if (pixel_fmt == libcamera::formats::YUV420) {
        cv::Mat yuv(height + height / 2, width, CV_8UC1, mapped);
        cv::cvtColor(yuv, gray, cv::COLOR_YUV2GRAY_I420);
    } else {
        spdlog::warn(
            "Unrecognised pixel format {}; treating as raw greyscale", pixel_fmt.toString());
        cv::Mat raw(height, width, CV_8UC1, mapped,
            static_cast<std::size_t>(m_rpi_cam.stream_cfg().stride));
        raw.copyTo(gray);
    }

    ::munmap(mapped, length);
    return gray;
}

cv::Mat OtvAnalyzer::scale_frame(const cv::Mat& src) const
{
    const float res = m_config.calibration.resolution;
    if (res >= 1.0f - 1e-4f)
        return src;

    cv::Mat dst;
    cv::resize(src, dst,
        cv::Size(static_cast<int>(static_cast<float>(src.cols) * res),
            static_cast<int>(static_cast<float>(src.rows) * res)));
    return dst;
}
