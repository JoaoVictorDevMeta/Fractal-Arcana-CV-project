#include "gesture/shape_classif.h"
#include <algorithm>
#include <cmath>
#include <cstdio>

struct Candidate
{
    Shape shape;
    float score;
};

float ShapeClassifier::clamp01(float x)
{
    return std::max(0.f, std::min(1.f, x));
}

// 1ª PREFERÊNCIA: Linha (simples em qualquer direção OU acúmulo de ida e volta ~180°)
float ShapeClassifier::scoreLine(ShapeFeatures &f)
{
    if (f.turns >= 1)
        return 0.0f;
    if (f.aspectRatio > 0.30f || f.maxPerpDev > 0.22f || f.hullCircularity > 0.42f)
        return 0.0f;

    float sThin = clamp01((0.30f - f.aspectRatio) / 0.30f);
    float sStraight = clamp01((0.22f - f.maxPerpDev) / 0.22f);

    return 0.72f + 0.14f * sThin + 0.14f * sStraight;
}

// 2ª PREFERÊNCIA: Escudo (Círculo)
float ShapeClassifier::scoreCircle(ShapeFeatures &f)
{
    if (f.endGapRatio > 0.35f || f.windingAngleDeg < 255.0f)
        return 0.0f;
    if (f.hullCircularity < 0.72f || f.aspectRatio < 0.45f)
        return 0.0f;

    float concavity = (f.hullArea > 1e-6) ? (float)(f.area / f.hullArea) : 0.0f;
    if (concavity < 0.78f)
        return 0.0f;

    float sHull = clamp01((f.hullCircularity - 0.72f) / 0.16f);
    float sAspect = clamp01((f.aspectRatio - 0.45f) / 0.28f);
    float sSweep = clamp01((f.windingAngleDeg - 255.0f) / 90.0f);
    float sGap = clamp01((0.35f - f.endGapRatio) / 0.30f);

    return 0.38f * sHull + 0.25f * sAspect + 0.22f * sSweep + 0.15f * sGap;
}

// 3ª PREFERÊNCIA: Raio (exige zigue-zague lateral real, sem colidir com linha de ida e volta)
float ShapeClassifier::scoreLightning(ShapeFeatures &f)
{
    if (f.closed || f.endGapRatio < 0.22f)
        return 0.0f;
    if (f.turns < 1 || f.polyVertices < 4 || f.polyVertices > 10)
        return 0.0f;
    if (f.windingAngleDeg > 240.0f)
        return 0.0f;
    if (f.aspectRatio <= 0.24f && f.maxPerpDev <= 0.18f)
        return 0.0f;

    float sTurns = (f.turns >= 2) ? 1.0f : 0.74f;
    float sZigZag = clamp01((f.maxPerpDev - 0.14f) / 0.16f);
    float sCorner = clamp01((f.cornerDefinition - 0.35f) / 0.25f);

    return 0.45f * sTurns + 0.35f * sZigZag + 0.20f * sCorner;
}

// 4ª PREFERÊNCIA: Triângulo
float ShapeClassifier::scoreTriangle(ShapeFeatures &f)
{
    if (!f.closed || f.endGapRatio > 0.24f)
        return 0.0f;
    if (f.numVertices < 3 || f.numVertices > 4)
        return 0.0f;
    if (f.hullCircularity > 0.74f || f.cornerDefinition < 0.48f)
        return 0.0f;
    if (f.altRadii && f.starTipRatio >= 0.55f)
        return 0.0f;

    float sVerts = (f.numVertices == 3) ? 1.0f : 0.65f;
    float sHull = clamp01((0.74f - f.hullCircularity) / 0.18f);
    float sCorner = clamp01((f.cornerDefinition - 0.48f) / 0.25f);
    float sRadial = clamp01((f.radialRatio - 1.45f) / 0.50f);

    return 0.35f * sVerts + 0.25f * sHull + 0.25f * sCorner + 0.15f * sRadial;
}

// 5ª PREFERÊNCIA: Estrela (pontas alternadas + tamanho das pontas parecido)
float ShapeClassifier::scoreStar(ShapeFeatures &f)
{
    if (!f.closed || f.endGapRatio > 0.24f || !f.altRadii)
        return 0.0f;
    if (f.starTipRatio < 0.55f)
        return 0.0f;

    float concavity = (f.hullArea > 1e-6) ? (float)(f.area / f.hullArea) : 1.0f;
    if (concavity > 0.74f)
        return 0.0f;

    float sConcave = clamp01((0.74f - concavity) / 0.25f);
    float sRadial = clamp01((f.radialRatio - 1.55f) / 0.80f);
    float sTips = clamp01((f.starTipRatio - 0.55f) / 0.30f);

    return 0.42f + 0.20f * sConcave + 0.18f * sRadial + 0.20f * sTips;
}

ShapeResult ShapeClassifier::classify(std::vector<cv::Point2f> &rawPoints)
{
    ShapeResult res;
    if (rawPoints.size() < 6)
    {
        res.resultText = "linha muito pequena";
        return res;
    }

    std::vector<cv::Point2f> formalizedPts = FeatureExtractor::equiDistPts(rawPoints, 64);
    if (formalizedPts.size() < 16)
    {
        res.resultText = "reamostragem falhou";
        return res;
    }

    std::vector<cv::Point2f> pts = FeatureExtractor::normalize(formalizedPts);
    ShapeFeatures f = extractor.extract(pts);

    char buf[320];
    std::snprintf(buf, sizeof(buf),
                  "closed=%d gap=%.2f wind=%.0f nV=%d hullC=%.2f asp=%.2f corner=%.2f turns=%d tipU=%.2f",
                  f.closed, f.endGapRatio, f.windingAngleDeg, f.numVertices,
                  f.hullCircularity, f.aspectRatio, f.cornerDefinition, f.turns, f.starTipRatio);
    res.resultText = buf;

    // Ordem estrita de preferência: Linha -> Escudo -> Raio -> Triângulo -> Estrela
    std::vector<Candidate> candidates = {
        {Shape::Linha, scoreLine(f)},
        {Shape::Circulo, scoreCircle(f)},
        {Shape::Raio, scoreLightning(f)},
        {Shape::Triangulo, scoreTriangle(f)},
        {Shape::Estrela, scoreStar(f)}};

    for (const auto &c : candidates)
    {
        if (c.score >= 0.55f)
        {
            res.shape = c.shape;
            res.confidence = c.score;
            return res;
        }
    }

    res.shape = Shape::Nenhum;
    res.confidence = 0.0f;
    return res;
}
