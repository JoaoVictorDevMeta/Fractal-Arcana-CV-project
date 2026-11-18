#include "gesture/formalizer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/geometry.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

// essa parte eu (jv) explico direitinho
// é uma função pra deixar pontos equidistantes
std::vector<cv::Point2f> FeatureExtractor::equiDistPts(std::vector<cv::Point2f>& pts, int n){
    if(pts.size() < 2 || n < 2 ){
        return {};
    }

    std::vector<float> acum(pts.size(), 0.0);
    for ( size_t i = 1; i < pts.size(); i++){
        acum[i] = acum[i - 1] + cv::norm(pts[i] - pts[i - 1]);
    } // comprimento acumulado

    float total = acum.back();
    if(total <= 1e-6f){
        return {};
    }

    float step = total / (n - 1);
    std::vector<cv::Point2f> out;
    out.reserve(n);
    out.push_back(pts.front());

    size_t j = 1;
    float target = step;
    // two-pointer de leve
    // ponteiros: i e j+target
    for(int i = 1; i < n-1; i++){
        while (j < pts.size() && acum[j] < target){
            j++;
        }

        if(j >= pts.size()) break;
        float seg = acum[j] - acum[j -1];
        float t = (seg > 1e-6f) ? (target - acum[j - 1]) / seg : 0.f;
        out.push_back(pts[j - 1] + t * (pts[j] - pts[j - 1]));
        target += step;
    }

    out.push_back(pts.back());
    return out;
}

std::vector<cv::Point2f> FeatureExtractor::normalize(std::vector<cv::Point2f>& pts){
    if (pts.empty()) return {};
    cv::Point2f c(0.0f, 0.0f);
    for (const auto& p : pts) c += p;
    c *= 1.f / (float)pts.size();

    float rMax = 0.0f;
    for (const auto& p : pts) rMax = std::max<float>(rMax, cv::norm(p - c));
    if (rMax < 1e-6) return {};

    std::vector<cv::Point2f> out(pts.size());
    for (size_t i = 0; i < pts.size(); ++i)
        out[i] = (pts[i] - c) / rMax;
    return out;
}

double FeatureExtractor::calculateCircularity(double area, double perimeter) {
    if (perimeter <= 1e-6) return 0.0;
    return 4.0 * CV_PI * area / (perimeter * perimeter);
}

float FeatureExtractor::calculateCornerMass(std::vector<cv::Point2f>& pts, bool closed) {
    int N = (int)pts.size();
    std::vector<float> absTurn(N, 0.f);
    
    for (int i = 0; i < N; ++i) {
        if (!closed && (i == 0 || i == N - 1)) continue;
        int ip = (i == 0) ? (closed ? N - 1 : -1) : i - 1;
        int in = (i == N - 1) ? (closed ? 0 : N) : i + 1;
        if (ip < 0 || in >= N) continue;

        cv::Point2f a = pts[i] - pts[ip];
        cv::Point2f b = pts[in] - pts[i];
        float cross = a.x * b.y - a.y * b.x;
        float dot = a.x * b.x + a.y * b.y;
        absTurn[i] = std::abs(std::atan2(cross, dot));
    }

    float total = std::accumulate(absTurn.begin(), absTurn.end(), 0.f);
    if (total < 1e-6f) return 0.f;

    std::vector<float> sortedTurns = absTurn;
    std::partial_sort(sortedTurns.begin(), sortedTurns.begin() + 4, sortedTurns.end(), std::greater<float>());
    
    return (sortedTurns[0] + sortedTurns[1] + sortedTurns[2] + sortedTurns[3]) / total;
}

float FeatureExtractor::calculateRadialRatio( std::vector<cv::Point2f>& pts) {
    int N = (int)pts.size();
    cv::Point2f c(0.f, 0.f);
    for ( auto& p : pts) c += p;
    c *= 1.f / (float)N;

    float rMin = 1e9f, rMax = 0.f;
    for ( auto& p : pts) {
        float r = cv::norm(p - c);
        rMin = std::min(rMin, r);
        rMax = std::max(rMax, r);
    }
    return (rMin > 1e-6f) ? rMax / rMin : 1.f;
}

ShapeFeatures FeatureExtractor::extract(std::vector<cv::Point2f>& pts) {
    ShapeFeatures f;
    int N = (int)pts.size();
    if (N < 8) return f;

    // Fechamento
    float gap = cv::norm(pts.front() - pts.back());
    double perOpen = cv::arcLength(pts, false);
    f.closed = (perOpen > 1e-6) ? (gap < 0.25 * (float)perOpen || gap < 0.15f) : (gap < 0.15f);

    // Área, perímetro e circularidade
    f.area = std::abs(cv::contourArea(pts));
    f.perimeter = cv::arcLength(pts, f.closed);
    f.circularity = calculateCircularity(f.area, f.perimeter);

    // Vértices (múltiplos epsilons)
    std::vector<int> counts;
    for (double k : {0.010, 0.015, 0.020, 0.025, 0.030, 0.035}) {
        std::vector<cv::Point2f> out;
        cv::approxPolyDP(pts, out, k * f.perimeter, f.closed);
        counts.push_back((int)out.size());
    }
    f.numVertices = counts[3];
    f.numFourVertices = (int)std::count(counts.begin(), counts.end(), 4);

    // Linha (fitLine)
    cv::Vec4f line;
    cv::fitLine(pts, line, cv::DIST_L2, 0, 0.01, 0.01);
    f.lineAngleDeg = std::atan2(line[1], line[0]) * 180.f / (float)CV_PI;
    cv::Point2f p0(line[2], line[3]);
    cv::Point2f dir(line[0], line[1]); 
    
    for (auto& p : pts) {
        cv::Point2f d = p - p0;
        float perp = std::abs(d.x * dir.y - d.y * dir.x);
        f.maxPerpDev = std::max(f.maxPerpDev, perp);
    }

    // Zigue-zague (turns)
    std::vector<float> absTurn(N, 0.f);
    int prevSign = 0;
    for (int i = 0; i < N; ++i) {
        if (!f.closed && (i == 0 || i == N - 1)) continue;
        int ip = (i == 0) ? (f.closed ? N - 1 : -1) : i - 1;
        int in = (i == N - 1) ? (f.closed ? 0 : N) : i + 1;
        if (ip < 0 || in >= N) continue;

        cv::Point2f a = pts[i] - pts[ip];
        cv::Point2f b = pts[in] - pts[i];
        float cross = a.x * b.y - a.y * b.x;
        float dot = a.x * b.x + a.y * b.y;
        float turn = std::atan2(cross, dot);
        absTurn[i] = std::abs(turn);

        if (absTurn[i] > 0.5f) {
            int s = (cross > 0) - (cross < 0);
            if (prevSign != 0 && s != prevSign) ++f.turns;
            prevSign = s;
        }
    }

    f.cornerDefinition = calculateCornerMass(pts, f.closed);
    f.radialRatio = calculateRadialRatio(pts);

    // Estrela (altRadii)
    cv::Point2f c(0.f, 0.f);
    for (auto& p : pts) c += p;
    c *= 1.f / (float)N;

    std::vector<cv::Point2f> poly;
    cv::approxPolyDP(pts, poly, 0.02 * f.perimeter, f.closed);
    if (poly.size() >= 5) {
        std::vector<float> r(poly.size());
        for (size_t i = 0; i < poly.size(); ++i) r[i] = cv::norm(poly[i] - c);
        int alt = 0;
        for (size_t i = 1; i + 1 < r.size(); ++i) {
            if ((r[i] > r[i - 1]) != (r[i + 1] > r[i])) ++alt;
        }
        f.altRadii = (float)alt / (float)(r.size() - 2) > 0.7f;
    }

    // Simetria de 4 pontas (Quadrado)
    if (f.closed && N >= 16) {
        std::vector<int> idx(N);
        std::iota(idx.begin(), idx.end(), 0);
        std::partial_sort(idx.begin(), idx.begin() + 4, idx.end(), [&](int a, int b) { return absTurn[a] > absTurn[b]; });
        
        std::vector<int> top4(idx.begin(), idx.begin() + 4);
        std::sort(top4.begin(), top4.end());
        
        float gaps[4];
        for (int i = 0; i < 4; ++i) {
            int g = top4[(i + 1) % 4] - top4[i];
            if (g < 0) g += N;
            gaps[i] = (float)g;
        }
        
        float mean = (gaps[0] + gaps[1] + gaps[2] + gaps[3]) / 4.f;
        float var = 0.f;
        for (float g : gaps) var += (g - mean) * (g - mean);
        float sd = std::sqrt(var / 4.f);
        f.fourFold = (mean > 0.5f) ? std::max(0.f, std::min(1.f, 1.f - sd / mean)) : 0.f;
    }

    return f;
}