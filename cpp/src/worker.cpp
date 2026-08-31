#include "yuvdiff/worker.hpp"

#include <algorithm>

namespace yuvdiff {

AsyncRenderWorker::AsyncRenderWorker(QObject* parent) : QThread(parent) {
    qRegisterMetaType<yuvdiff::FrameReadyData>("yuvdiff::FrameReadyData");
    start();
}

AsyncRenderWorker::~AsyncRenderWorker() {
    stop();
    wait();
}

void AsyncRenderWorker::stop() {
    running_ = false;
    queue_cv_.notify_all();
}

void AsyncRenderWorker::set_parsers(
    std::shared_ptr<YUVParser> parser_a,
    std::shared_ptr<YUVParser> parser_b,
    std::shared_ptr<Renderer> renderer
) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    parser_a_ = parser_a;
    parser_b_ = parser_b;
    renderer_ = renderer;
    cache_.clear();
    pending_interactive_request_.reset();
    while (!prefetch_queue_.empty()) prefetch_queue_.pop();
}

void AsyncRenderWorker::clear_cache() {
    cache_.clear();
}

void AsyncRenderWorker::request_frame(int frame_idx, RenderMode mode, int threshold, bool is_playing) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    is_playing_ = is_playing;
    pending_interactive_request_ = Request{frame_idx, mode, threshold, false};

    // If currently playing, enqueue prefetch for next frames
    if (is_playing) {
        size_t max_frames = 0;
        if (parser_a_ && parser_b_) {
            max_frames = std::min(parser_a_->num_frames(), parser_b_->num_frames());
        } else if (parser_a_) {
            max_frames = parser_a_->num_frames();
        } else if (parser_b_) {
            max_frames = parser_b_->num_frames();
        }

        if (max_frames > 0) {
            while (!prefetch_queue_.empty()) prefetch_queue_.pop();
            for (int offset = 1; offset <= 3; ++offset) {
                int next_idx = (frame_idx + offset) % static_cast<int>(max_frames);
                prefetch_queue_.push(Request{next_idx, mode, threshold, true});
            }
        }
    }

    queue_cv_.notify_one();
}

void AsyncRenderWorker::run() {
    while (running_) {
        Request current_request;
        bool has_request = false;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this]() {
                return !running_ || pending_interactive_request_.has_value() || !prefetch_queue_.empty();
            });

            if (!running_) break;

            if (pending_interactive_request_.has_value()) {
                current_request = *pending_interactive_request_;
                pending_interactive_request_.reset();
                has_request = true;
            } else if (!prefetch_queue_.empty()) {
                current_request = prefetch_queue_.front();
                prefetch_queue_.pop();
                has_request = true;
            }
        }

        if (has_request) {
            process_request(current_request);
        }
    }
}

void AsyncRenderWorker::process_request(const Request& req) {
    std::shared_ptr<YUVParser> pa;
    std::shared_ptr<YUVParser> pb;
    std::shared_ptr<Renderer> rend;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        pa = parser_a_;
        pb = parser_b_;
        rend = renderer_;
    }

    if (!pa && !pb) return;

    if (req.frame_idx < 0) return;
    if (pa && static_cast<size_t>(req.frame_idx) >= pa->num_frames()) return;
    if (pb && static_cast<size_t>(req.frame_idx) >= pb->num_frames()) return;

    // 1. Check cache
    auto cached = cache_.get(req.frame_idx, req.mode, req.threshold);
    if (cached.has_value()) {
        if (!req.is_prefetch) {
            FrameReadyData data;
            data.frame_idx = cached->frame_idx;
            data.is_dual = (pa && pb);
            data.single_channel = pa ? "a" : "b";
            data.frame_a = cached->frame_a;
            data.frame_b = cached->frame_b;
            data.mode = cached->mode;
            data.threshold = cached->threshold;
            data.psnr = cached->psnr;
            data.ssim = cached->ssim;
            data.diff_pixels = cached->diff_pixels;
            data.total_pixels = cached->total_pixels;
            data.diff_ratio = cached->diff_ratio;
            data.diff_gt_half_t = cached->diff_gt_half_t;
            data.diff_gt_half_t_ratio = cached->diff_gt_half_t_ratio;
            data.diff_gt_t = cached->diff_gt_t;
            data.diff_gt_t_ratio = cached->diff_gt_t_ratio;
            data.diff_gt_2t = cached->diff_gt_2t;
            data.diff_gt_2t_ratio = cached->diff_gt_2t_ratio;
            data.diff_mean = cached->diff_mean;
            data.diff_median = cached->diff_median;
            data.diff_max = cached->diff_max;
            data.diff_min = cached->diff_min;
            emit frameReady(data);
        }
        return;
    }

    // 2. Compute if not in cache
    try {
        if (pa && pb) {
            auto fa = std::make_shared<YUVFrame>(pa->read_frame(static_cast<size_t>(req.frame_idx)));
            auto fb = std::make_shared<YUVFrame>(pb->read_frame(static_cast<size_t>(req.frame_idx)));

            diff_engine_.set_threshold(req.threshold);
            PSNRResult psnr = metrics_calc_.psnr(*fa, *fb);
            DiffResult diff = diff_engine_.diff(*fa, *fb);
            SSIMResult ssim;
            if (!is_playing_) {
                ssim = metrics_calc_.ssim(*fa, *fb);
            }

            int64_t diff_pixels = diff.diff_pixel_count;
            int64_t total_pixels = diff.total_pixel_count;

            CachedFrame item;
            item.frame_idx = req.frame_idx;
            item.mode = req.mode;
            item.threshold = req.threshold;
            item.frame_a = fa;
            item.frame_b = fb;
            item.psnr = psnr;
            item.ssim = ssim;
            item.diff_pixels = diff_pixels;
            item.total_pixels = total_pixels;
            item.diff_ratio = diff.diff_ratio;
            item.diff_gt_half_t = diff.diff_gt_half_t;
            item.diff_gt_half_t_ratio = diff.diff_gt_half_t_ratio;
            item.diff_gt_t = diff.diff_gt_t;
            item.diff_gt_t_ratio = diff.diff_gt_t_ratio;
            item.diff_gt_2t = diff.diff_gt_2t;
            item.diff_gt_2t_ratio = diff.diff_gt_2t_ratio;
            item.diff_mean = diff.diff_mean;
            item.diff_median = diff.diff_median;
            item.diff_max = diff.diff_max;
            item.diff_min = diff.diff_min;
            cache_.put(item);

            if (!req.is_prefetch) {
                FrameReadyData data;
                data.frame_idx = req.frame_idx;
                data.is_dual = true;
                data.frame_a = fa;
                data.frame_b = fb;
                data.mode = req.mode;
                data.threshold = req.threshold;
                data.psnr = psnr;
                data.ssim = ssim;
                data.diff_pixels = diff_pixels;
                data.total_pixels = total_pixels;
                data.diff_ratio = diff.diff_ratio;
                data.diff_gt_half_t = diff.diff_gt_half_t;
                data.diff_gt_half_t_ratio = diff.diff_gt_half_t_ratio;
                data.diff_gt_t = diff.diff_gt_t;
                data.diff_gt_t_ratio = diff.diff_gt_t_ratio;
                data.diff_gt_2t = diff.diff_gt_2t;
                data.diff_gt_2t_ratio = diff.diff_gt_2t_ratio;
                data.diff_mean = diff.diff_mean;
                data.diff_median = diff.diff_median;
                data.diff_max = diff.diff_max;
                data.diff_min = diff.diff_min;
                emit frameReady(data);
            }
        } else if (pa) {
            auto fa = std::make_shared<YUVFrame>(pa->read_frame(static_cast<size_t>(req.frame_idx)));

            CachedFrame item;
            item.frame_idx = req.frame_idx;
            item.mode = req.mode;
            item.threshold = req.threshold;
            item.frame_a = fa;
            item.total_pixels = static_cast<int64_t>(fa->width) * fa->height;
            cache_.put(item);

            if (!req.is_prefetch) {
                FrameReadyData data;
                data.frame_idx = req.frame_idx;
                data.is_dual = false;
                data.single_channel = "a";
                data.frame_a = fa;
                data.mode = req.mode;
                data.threshold = req.threshold;
                data.total_pixels = item.total_pixels;
                emit frameReady(data);
            }
        } else if (pb) {
            auto fb = std::make_shared<YUVFrame>(pb->read_frame(static_cast<size_t>(req.frame_idx)));

            CachedFrame item;
            item.frame_idx = req.frame_idx;
            item.mode = req.mode;
            item.threshold = req.threshold;
            item.frame_b = fb;
            item.total_pixels = static_cast<int64_t>(fb->width) * fb->height;
            cache_.put(item);

            if (!req.is_prefetch) {
                FrameReadyData data;
                data.frame_idx = req.frame_idx;
                data.is_dual = false;
                data.single_channel = "b";
                data.frame_b = fb;
                data.mode = req.mode;
                data.threshold = req.threshold;
                data.total_pixels = item.total_pixels;
                emit frameReady(data);
            }
        }
    } catch (const std::exception& e) {
        if (!req.is_prefetch) {
            emit renderError(req.frame_idx, QString::fromStdString(e.what()));
        }
    }
}

} // namespace yuvdiff
