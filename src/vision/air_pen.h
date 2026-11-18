#pragma once
#include "vision/wand_detector.h"
#include "vision/stroke_assistant.h"
#include <opencv2/core.hpp>
#include <vector>

class AirPen {
public:
    cv::Mat canvas;
    bool penDown = true;
    float alpha = 0.50f;
    float minStepDist = 8.0f;
    int maxMissed = 12;
    int thickness = 4;
    cv::Scalar color{0, 255, 255};

    void reset(cv::Size size);
    void update(Detection& d);
    void renderAssistant(cv::Mat& frame);

    std::vector<cv::Point2f> getStroke();
    bool hasFinishedStroke();
    void resetStroke();

private:
    StrokeAssistant assistant_;
    cv::Point2f last_{0, 0}, smooth_{0, 0};
    bool hasLast_ = false, hasSmooth_ = false;
    int missed_ = 0;

    std::vector<cv::Point2f> currentStroke_;
    bool strokePending_ = false;

    cv::Point2f idleAnchor_{0, 0};
    int idleFrames_ = 0;
    int MAX_IDLE_FRAMES = 12;
    float IDLE_RADIUS = 20.0f;
};