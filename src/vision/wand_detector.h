#pragma once
#include "core/hsv_range.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>

struct Detection {
    bool found = false;
    cv::Point2f center{0.f, 0.f};
    double area = 0.0;
    cv::Rect bbox;
};

class WandDetector {
public:
    HsvRange range;
    double minArea = 30.0;
    Detection detect(const cv::Mat& bgr, cv::Mat* maskOut = nullptr) const;
private:
    cv::Mat kernel_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5,5));
};