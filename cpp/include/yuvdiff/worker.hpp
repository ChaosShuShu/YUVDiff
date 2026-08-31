#pragma once

#include "yuvdiff/cache.hpp"
#include "yuvdiff/diff.hpp"
#include "yuvdiff/metrics.hpp"
#include "yuvdiff/parser.hpp"
#include "yuvdiff/renderer.hpp"

#include <QObject>
#include <QThread>

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>

namespace yuvdiff {

struct FrameReadyData {
    int frame_idx = 0;
    bool is_dual = false;
    std::string single_channel; // "a" or "b"
    std::shared_ptr<YUVFrame> frame_a;
    std::shared_ptr<YUVFrame> frame_b;
    RenderMode mode = RenderMode::ORIGINAL_A;
    int threshold = 4;
    PSNRResult psnr;
    SSIMResult ssim;
    int64_t diff_pixels = 0;
    int64_t total_pixels = 0;
    double diff_ratio = 0.0;
    int64_t diff_gt_t = 0;
    double diff_gt_t_ratio = 0.0;
    int64_t diff_gt_half_t = 0;
    double diff_gt_half_t_ratio = 0.0;
    int64_t diff_gt_2t = 0;
    double diff_gt_2t_ratio = 0.0;
    double diff_mean = 0.0;
    double diff_median = 0.0;
    int32_t diff_max = 0;
    int32_t diff_min = 0;
};

class AsyncRenderWorker : public QThread {
    Q_OBJECT

public:
    explicit AsyncRenderWorker(QObject* parent = nullptr);
    ~AsyncRenderWorker() override;

    void set_parsers(
        std::shared_ptr<YUVParser> parser_a,
        std::shared_ptr<YUVParser> parser_b,
        std::shared_ptr<Renderer> renderer
    );

    // Request a frame to be rendered and displayed on UI
    void request_frame(int frame_idx, RenderMode mode, int threshold, bool is_playing = false);

    // Stop worker thread
    void stop();

    // Clear internal cache
    void clear_cache();

signals:
    void frameReady(const yuvdiff::FrameReadyData& data);
    void renderError(int frame_idx, const QString& error_msg);

protected:
    void run() override;

private:
    struct Request {
        int frame_idx = 0;
        RenderMode mode = RenderMode::ORIGINAL_A;
        int threshold = 4;
        bool is_prefetch = false;
    };

    std::shared_ptr<YUVParser> parser_a_;
    std::shared_ptr<YUVParser> parser_b_;
    std::shared_ptr<Renderer> renderer_;

    FrameCache cache_{64};
    DiffEngine diff_engine_{4};
    MetricsCalculator metrics_calc_;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::optional<Request> pending_interactive_request_;
    std::queue<Request> prefetch_queue_;
    std::atomic<bool> running_{true};
    std::atomic<bool> is_playing_{false};

    void process_request(const Request& req);
};

} // namespace yuvdiff

Q_DECLARE_METATYPE(yuvdiff::FrameReadyData)
