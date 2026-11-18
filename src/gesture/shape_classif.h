#pragma once
#include "gesture/shapes.h"

class ShapeClassifier {
    private:
        
    public:
        ShapeResult classify(const std::vector<cv::Point2f>& rawPoints) const;
};