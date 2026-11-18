#include "gesture/shape_classif.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include <opencv2/core.hpp>
#include <opencv2/geometry.hpp>
#include "gesture/shapes.h"
#include "gesture/formalizer.h"

struct Candidate {
    Shape shape;
    float score;
};

float clamp01(float x) { return std::max(0.f, std::min(1.f, x)); }

bool ShapeClassifier::isHorizontalLine( ShapeFeatures& f) {
    bool isHorizontalAngle = (std::abs(f.lineAngleDeg) < 20.f || std::abs(f.lineAngleDeg) > 160.f);
    return !f.closed && f.numVertices <= 3 && isHorizontalAngle && f.maxPerpDev < 0.10f && f.turns <= 1;
}

bool ShapeClassifier::isLightning( ShapeFeatures& f) {
    return !f.closed && f.turns >= 4 && f.numVertices >= 5;
}

ShapeResult ShapeClassifier::classifyClosedShape( ShapeFeatures& f) {
    ShapeResult res;
    if (!f.closed) return res;
    
    float sCircle = 0.35f * clamp01((0.25f - f.cornerDefinition) / 0.15f)
                  + 0.30f * clamp01((1.20f - f.radialRatio) / 0.15f)
                  + 0.15f * (f.numFourVertices == 0 ? 1.f : 0.f)
                  + 0.10f * clamp01((f.circularity - 0.75f) / 0.15f)
                  + 0.10f * clamp01((0.20f - f.fourFold) / 0.20f);

    float sSquare = 0.f;
    if (f.numVertices >= 3 && f.numVertices <= 6) {
        sSquare = 0.35f * clamp01((f.cornerDefinition - 0.30f) / 0.30f)
                + 0.30f * clamp01((f.radialRatio - 1.15f) / 0.30f)
                + 0.15f * clamp01((float)f.numFourVertices / 4.f)
                + 0.10f * clamp01((0.85f - f.circularity) / 0.10f)
                + 0.10f * clamp01((f.fourFold - 0.20f) / 0.20f);
    }

    float sTriangle = 0.f;
    if (f.numVertices == 3) {
        sTriangle = 0.6f
                  + 0.20f * clamp01((f.radialRatio - 1.5f) / 0.5f)
                  + 0.20f * clamp01((0.65f - f.circularity) / 0.15f);
    }

    float sStar = 0.f;
    if (f.numVertices >= 8 && f.altRadii) {
        sStar = 0.6f
              + 0.20f * clamp01((f.radialRatio - 1.5f) / 1.0f)
              + 0.20f * clamp01((f.cornerDefinition - 0.30f) / 0.40f);
    }

    std::vector<Candidate> candidates = {
        {Shape::Circulo,   sCircle},
        {Shape::Quadrado,   sSquare},
        {Shape::Triangulo, sTriangle},
        {Shape::Estrela,     sStar}
    };

    auto best = std::max_element(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            return a.score < b.score;
        });

    // Se o melhor score for muito baixo, rejeita o gesto
    if (best->score < 0.55f) {
        res.shape = Shape::Nenhum;
        res.confidence = best->score;
    } else {
        res.shape = best->shape;
        res.confidence = best->score;
    }
    
    return res;
}

ShapeResult ShapeClassifier::classify(std::vector<cv::Point2f>& rawPoints) {
    ShapeResult res;
    if (rawPoints.size() < 8) { res.resultText = "linha muito pequena"; return res; }

    // Pipeline do README: Reamostragem -> Normalização -> Extração
    std::vector<cv::Point2f> formalizedPts = FeatureExtractor::equiDistPts(rawPoints, 64);
    if (formalizedPts.size() < 16) { res.resultText = "reamostragem falhou"; return res; }
    
    std::vector<cv::Point2f> pts = FeatureExtractor::normalize(formalizedPts);
    ShapeFeatures f = extractor.extract(pts);

    // Debug string (exatamente como no seu código original)
    char buf[320];
    std::snprintf(buf, sizeof(buf), "closed=%d nV=%d n4=%d circ=%.2f ang=%.0f perp=%.2f corner=%.2f rR=%.2f turns=%d altR=%d four=%.2f",
        f.closed, f.numVertices, f.numFourVertices, f.circularity, f.lineAngleDeg, f.maxPerpDev, f.cornerDefinition, f.radialRatio, f.turns, f.altRadii ? 1 : 0, f.fourFold);
    res.resultText = buf;

    // --- 1. Regras de Formas Abertas ---
    if (isHorizontalLine(f)) {
        res.shape = Shape::Linha;
        res.confidence = 0.6f + 0.4f * clamp01((0.10f - f.maxPerpDev) / 0.10f);
        return res;
    }

    if (isLightning(f)) {
        res.shape = Shape::Raio;
        res.confidence = 0.6f + 0.4f * clamp01((f.turns - 4) / 6.f);
        return res;
    }

    // --- 2. Regras de Formas Fechadas ---
    if (f.closed) {
        return classifyClosedShape(f);
    }

    // Se chegou aqui, não é linha, nem raio, nem forma fechada.
    res.shape = Shape::Nenhum;
    res.confidence = 0.0f;
    return res;
}