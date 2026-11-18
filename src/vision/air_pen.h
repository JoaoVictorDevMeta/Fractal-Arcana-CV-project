#pragma once
#include "vision/wand_detector.h"
#include <opencv2/core.hpp>

class AirPen {
public:
    cv::Mat canvas;
    bool penDown = true;
    float alpha = 0.5f;
    int maxMissed = 15;
    int thickness = 4;
    cv::Scalar color{0, 255, 255};

    void reset(const cv::Size& size);
    void update(const Detection& d);

    std::vector<cv::Point2f> getStroke();
    bool hasFinishedStroke();
    void resetStroke();
private:
    cv::Point2f last_{0,0}, smooth_{0,0};
    bool hasLast_ = false, hasSmooth_ = false;
    int missed_ = 0;

    std::vector<cv::Point2f> currentStroke_;
    bool strokePending_ = false;

    // ---------------------------
    // ajuste do tempo e frames de reconhecimento
    // ---------------------------
    cv::Point2f idleAnchor_{0,0}; 
    int idleFrames_ = 0;
    const int MAX_IDLE_FRAMES = 20; 
    const float IDLE_RADIUS = 15.0f;
};