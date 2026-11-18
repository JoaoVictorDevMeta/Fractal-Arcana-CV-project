#pragma once
#include "gesture/shapes.h"

class FeatureExtractor {
    private:
        double calculateCircularity(double area, double perimeter);
        float calculateCornerMass(std::vector<cv::Point2f>& pts, bool closed);
        float calculateRadialRatio(std::vector<cv::Point2f>& pts);

    public:
        static std::vector<cv::Point2f> equiDistPts( std::vector<cv::Point2f>& pts, int n = 64);
        static std::vector<cv::Point2f> normalize( std::vector<cv::Point2f>& pts);

        ShapeFeatures extract(std::vector<cv::Point2f>& pts);
};

