#pragma once

#include "yuvdiff/diff.hpp"
#include "yuvdiff/metrics.hpp"
#include "yuvdiff/renderer.hpp"

#include <list>
#include <memory>
#include <mutex>
#include <optional>
#include <tuple>
#include <unordered_map>

namespace yuvdiff {

struct CachedFrame {
    int frame_idx = 0;
    RenderMode mode = RenderMode::ORIGINAL_A;
    int threshold = 4;
    std::shared_ptr<YUVFrame> frame_a;
    std::shared_ptr<YUVFrame> frame_b;
    RgbImage rgb;
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

class FrameCache {
public:
    explicit FrameCache(size_t max_capacity = 64) : max_capacity_(max_capacity) {}

    std::optional<CachedFrame> get(int frame_idx, RenderMode mode, int threshold) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto key = std::make_tuple(frame_idx, mode, threshold);
        auto it = map_.find(key);
        if (it == map_.end()) {
            return std::nullopt;
        }
        // Move accessed item to front of list (LRU)
        lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
        return *(it->second);
    }

    void put(const CachedFrame& item) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto key = std::make_tuple(item.frame_idx, item.mode, item.threshold);
        auto it = map_.find(key);
        if (it != map_.end()) {
            // Update item and move to front
            *(it->second) = item;
            lru_list_.splice(lru_list_.begin(), lru_list_, it->second);
            return;
        }

        // Evict oldest if capacity exceeded
        if (lru_list_.size() >= max_capacity_) {
            const auto& oldest = lru_list_.back();
            auto oldest_key = std::make_tuple(oldest.frame_idx, oldest.mode, oldest.threshold);
            map_.erase(oldest_key);
            lru_list_.pop_back();
        }

        lru_list_.push_front(item);
        map_[key] = lru_list_.begin();
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        map_.clear();
        lru_list_.clear();
    }

    size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lru_list_.size();
    }

private:
    struct TupleHash {
        size_t operator()(const std::tuple<int, RenderMode, int>& t) const {
            auto h1 = std::hash<int>{}(std::get<0>(t));
            auto h2 = std::hash<int>{}(static_cast<int>(std::get<1>(t)));
            auto h3 = std::hash<int>{}(std::get<2>(t));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    size_t max_capacity_ = 64;
    mutable std::mutex mutex_;
    std::list<CachedFrame> lru_list_;
    std::unordered_map<std::tuple<int, RenderMode, int>, std::list<CachedFrame>::iterator, TupleHash> map_;
};

} // namespace yuvdiff
