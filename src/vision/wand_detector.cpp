#include "vision/wand_detector.h"
#include <opencv2/geometry.hpp>
#include <vector>

Detection WandDetector::detect(cv::Mat& bgr, cv::Mat* maskOut) {
    cv::Mat blurred, hsv, mask;
    cv::GaussianBlur(bgr, blurred, cv::Size(5, 5), 0);
    cv::cvtColor(blurred, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv,
                cv::Scalar(range.hLo, range.sLo, range.vLo),
                cv::Scalar(range.hHi, range.sHi, range.vHi),
                mask);

    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, kernel_);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, kernel_);
    if (maskOut) *maskOut = mask;

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    int best = -1;
    double bestArea = 0.0;
    for (size_t i = 0; i < contours.size(); ++i) {
        double a = cv::contourArea(contours[i]);
        if (a > bestArea) {
            bestArea = a;
            best = (int)i;
        }
    }

    Detection d;
    if (best < 0 || bestArea < minArea) return d;

    cv::Moments m = cv::moments(contours[best]);
    if (m.m00 <= 0.0) return d;

    d.found = true;
    d.area = bestArea;
    d.center = {float(m.m10 / m.m00), float(m.m01 / m.m00)};
    d.bbox = cv::boundingRect(contours[best]);
    return d;
}