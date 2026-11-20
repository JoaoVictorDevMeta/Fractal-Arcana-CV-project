#pragma once
#include "gesture/shapes.h"
#include "gesture/formalizer.h"
#include <vector>

class ShapeClassifier {
private:
    FeatureExtractor extractor;

    float clamp01(float x);
    float scoreLine(ShapeFeatures& f);
    float scoreLightning(ShapeFeatures& f);
    float scoreCircle(ShapeFeatures& f);
    float scoreTriangle(ShapeFeatures& f);
    float scoreStar(ShapeFeatures& f);

public:
    ShapeResult classify(std::vector<cv::Point2f>& rawPoints);
};