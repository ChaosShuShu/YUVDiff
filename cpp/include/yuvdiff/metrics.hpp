#pragma once

#include "yuvdiff/parser.hpp"

namespace yuvdiff {

struct PSNRResult {
    double y = 0.0;
    double u = 0.0;
    double v = 0.0;
    double total = 0.0;
};

struct SSIMResult {
    double y = 0.0;
};

class MetricsCalculator {
public:
    PSNRResult psnr(const YUVFrame& a, const YUVFrame& b) const;
    SSIMResult ssim(const YUVFrame& a, const YUVFrame& b) const;
};

} // namespace yuvdiff
