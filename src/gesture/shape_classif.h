#pragma once
#include "gesture/shapes.h"
#include "gesture/formalizer.h"

class ShapeClassifier {
    private:
        FeatureExtractor extractor;
        
        bool isHorizontalLine( ShapeFeatures& f);
        bool isLightning( ShapeFeatures& f);
        ShapeResult classifyClosedShape( ShapeFeatures& f);
    public:
        ShapeResult classify( std::vector<cv::Point2f>& rawPoints);
};