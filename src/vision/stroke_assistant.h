#pragma once
#include <opencv2/core.hpp>
#include <vector>

class StrokeAssistant {
public:
    float snapRadius = 60.0f;

    std::vector<cv::Point2f> smoothStroke(std::vector<cv::Point2f>& raw, int passes = 2);
    float totalLength(std::vector<cv::Point2f>& pts);
    void renderOverlay(cv::Mat& frame, std::vector<cv::Point2f>& stroke);
};