#include "vision/stroke_assistant.h"
#include <opencv2/imgproc.hpp>

std::vector<cv::Point2f> StrokeAssistant::smoothStroke(std::vector<cv::Point2f>& raw, int passes) {
    if (raw.size() < 4) return raw;
    std::vector<cv::Point2f> curr = raw;

    for (int p = 0; p < passes; ++p) {
        std::vector<cv::Point2f> next = curr;
        for (size_t i = 1; i + 1 < curr.size(); ++i) {
            next[i] = 0.25f * curr[i - 1] + 0.50f * curr[i] + 0.25f * curr[i + 1];
        }
        curr = next;
    }
    return curr;
}

float StrokeAssistant::totalLength(std::vector<cv::Point2f>& pts) {
    float len = 0.0f;
    for (size_t i = 1; i < pts.size(); ++i) {
        len += cv::norm(pts[i] - pts[i - 1]);
    }
    return len;
}

void StrokeAssistant::renderOverlay(cv::Mat& frame, std::vector<cv::Point2f>& stroke) {
    if (stroke.size() < 2) return;

    std::vector<cv::Point2f> smooth = smoothStroke(stroke, 2);
    for (size_t i = 1; i < smooth.size(); ++i) {
        cv::Point p1(cvRound(smooth[i - 1].x), cvRound(smooth[i - 1].y));
        cv::Point p2(cvRound(smooth[i].x), cvRound(smooth[i].y));
        cv::line(frame, p1, p2, cv::Scalar(255, 200, 0), 6, cv::LINE_AA);
        cv::line(frame, p1, p2, cv::Scalar(255, 255, 255), 2, cv::LINE_AA);
    }

    if (stroke.size() > 8 && totalLength(stroke) > 120.0f) {
        cv::Point startPt(cvRound(stroke.front().x), cvRound(stroke.front().y));
        cv::Point currPt(cvRound(stroke.back().x), cvRound(stroke.back().y));
        float distToStart = (float)cv::norm(stroke.back() - stroke.front());

        bool nearClose = distToStart <= snapRadius;
        cv::Scalar guideColor = nearClose ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 180, 255);

        cv::circle(frame, startPt, (int)snapRadius, guideColor, 1, cv::LINE_AA);
        cv::circle(frame, startPt, 6, guideColor, -1, cv::LINE_AA);

        if (nearClose) {
            cv::line(frame, currPt, startPt, cv::Scalar(0, 255, 0), 2, cv::LINE_AA);
        }
    }
}