#include "vision/air_pen.h"
#include <opencv2/imgproc.hpp>

void AirPen::reset(const cv::Size& size) {
    canvas = cv::Mat::zeros(size, CV_8UC3);
    hasLast_ = hasSmooth_ = false;
    missed_ = 0;
}

void AirPen::update(const Detection& d) {
    if (canvas.empty()) return;
    if (!penDown) { hasLast_ = hasSmooth_ = false; return; }
    if (!d.found) {
        if (++missed_ > maxMissed) hasLast_ = hasSmooth_ = false;
        return;
    }
    missed_ = 0;
    cv::Point2f p = d.center;
    if (hasSmooth_) p = alpha * d.center + (1.f - alpha) * smooth_;
    smooth_ = p; hasSmooth_ = true;
    if (hasLast_) {
        cv::line(canvas,
                 cv::Point(cvRound(last_.x), cvRound(last_.y)),
                 cv::Point(cvRound(p.x),    cvRound(p.y)),
                 color, thickness, cv::LINE_AA);
    }
    last_ = p; hasLast_ = true;
}