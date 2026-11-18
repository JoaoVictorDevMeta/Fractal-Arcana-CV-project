#include <algorithm>
#include <cmath>
#include <cstdio>
#include <numeric>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/geometry.hpp>
#include "gesture/shapes.h"
#include "gesture/formalizer.h"

struct Features
{
    bool closed = false;
    int numVertices = 0;
    double area = 0.0;
    double perimeter = 0.0;
    float circularity = 0.0;
    float lineAngleDeg = 0.0;
    float maxPerpDev = 0.0;
    float cornerDefinition = 0.0;
    float radialRatio = 0.0;
};

Features defineFeat(std::vector<cv::Point2f> &pts)
{
    Features f;
    int N = (int)pts.size();
    if (N < 8)
        return f;

    float distPT = cv::norm(pts.front() - pts.back());
    double lineLength = cv::arcLength(pts, false);
    f.closed = (lineLength > 1e-6) ? (distPT < 0.25 * (float)lineLength || distPT < 0.15) : (distPT < 0.15);

    return f; // Adicionado para não quebrar temporariamente
};

ShapeResult classify(std::vector<cv::Point2f> &rawPts)
{
    ShapeResult res;
    if (rawPts.size() < 8)
    {
        res.resultText = "stroke too short";
        return res;
    }

    std::vector<cv::Point2f> formalizedPts = FeatureExtractor::equiDistPts(rawPts);
    std::vector<cv::Point2f> pts = FeatureExtractor::normalize(formalizedPts);
    Features f = defineFeat(formalizedPts);

    ShapeResult result{};

    return result; // Adicionado para não quebrar temporariamente
}
