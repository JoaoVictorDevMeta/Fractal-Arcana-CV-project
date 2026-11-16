#pragma once
#include "vision/wand_detector.h"
#include <opencv2/core.hpp>

class AirPen {
public:
    cv::Mat canvas;
    bool penDown = true;
    float alpha = 0.5f;
    int maxMissed = 5;
    int thickness = 4;
    cv::Scalar color{0, 255, 255};

    void reset(const cv::Size& size);
    void update(const Detection& d);
private:
    cv::Point2f last_{0,0}, smooth_{0,0};
    bool hasLast_ = false, hasSmooth_ = false;
    int missed_ = 0;
};