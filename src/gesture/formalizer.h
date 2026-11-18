#pragma once
#include "gesture/shapes.h"

class FeatureExtractor {
    private:
        
    public:
        static std::vector<cv::Point2f> equiDistPts(const std::vector<cv::Point2f>& pts, int n = 64);
        static std::vector<cv::Point2f> normalize(const std::vector<cv::Point2f>& pts);

        ShapeFeatures extract(std::vector<cv::Point2f>& pts);
};

