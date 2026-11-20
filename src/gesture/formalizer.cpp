#include "gesture/formalizer.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/core.hpp>
#include <opencv2/geometry.hpp>
#include <algorithm>
#include <cmath>
#include <numeric>

std::vector<cv::Point2f> FeatureExtractor::equiDistPts(std::vector<cv::Point2f> &pts, int n)
{
    if (pts.size() < 2 || n < 2)
        return {};

    std::vector<float> acum(pts.size(), 0.0);
    for (size_t i = 1; i < pts.size(); i++)
    {
        acum[i] = acum[i - 1] + static_cast<float>(cv::norm(pts[i] - pts[i - 1]));
    } // comprimento acumulado

    float total = acum.back();
    if (total <= 1e-6f)
    {
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
    for (int i = 1; i < n - 1; i++)
    {
        while (j < pts.size() && acum[j] < target)
        {
            j++;
        }

        if (j >= pts.size())
            break;
        float seg = acum[j] - acum[j - 1];
        float t = (seg > 1e-6f) ? (target - acum[j - 1]) / seg : 0.f;
        out.push_back(pts[j - 1] + t * (pts[j] - pts[j - 1]));
        target += step;
    }

    out.push_back(pts.back());
    return out;
}

std::vector<cv::Point2f> FeatureExtractor::normalize(std::vector<cv::Point2f> &pts)
{
    if (pts.empty())
        return {};
    cv::Point2f c(0.0f, 0.0f);
    for (auto &p : pts)
        c += p;
    c *= 1.f / (float)pts.size();

    float rMax = 0.0f;
    for (auto &p : pts)
        rMax = std::max<float>(rMax, static_cast<float>(cv::norm(p - c)));
    if (rMax < 1e-6f)
        return {};

    std::vector<cv::Point2f> out(pts.size());
    for (size_t i = 0; i < pts.size(); ++i)
    {
        out[i] = (pts[i] - c) / rMax;
    }
    return out;
}

double FeatureExtractor::calculateCircularity(double area, double perimeter)
{
    if (perimeter <= 1e-6)
        return 0.0;
    return 4.0 * CV_PI * area / (perimeter * perimeter);
}

float FeatureExtractor::calculateCornerMass(std::vector<cv::Point2f> &pts, bool closed)
{
    int N = (int)pts.size();
    std::vector<float> absTurn(N, 0.f);

    for (int i = 0; i < N; ++i)
    {
        if (!closed && (i == 0 || i == N - 1))
            continue;
        int ip = (i == 0) ? (closed ? N - 1 : -1) : i - 1;
        int in = (i == N - 1) ? (closed ? 0 : N) : i + 1;
        if (ip < 0 || in >= N)
            continue;

        cv::Point2f a = pts[i] - pts[ip];
        cv::Point2f b = pts[in] - pts[i];
        float cross = a.x * b.y - a.y * b.x;
        float dot = a.x * b.x + a.y * b.y;
        absTurn[i] = std::abs(std::atan2(cross, dot));
    }

    float total = std::accumulate(absTurn.begin(), absTurn.end(), 0.f);
    if (total < 1e-6f)
        return 0.f;

    std::vector<float> sortedTurns = absTurn;
    std::partial_sort(sortedTurns.begin(), sortedTurns.begin() + 4, sortedTurns.end(), std::greater<float>());
    return (sortedTurns[0] + sortedTurns[1] + sortedTurns[2] + sortedTurns[3]) / total;
}

float FeatureExtractor::calculateRadialRatio(std::vector<cv::Point2f> &pts)
{
    int N = (int)pts.size();
    cv::Point2f c(0.f, 0.f);
    for (auto &p : pts)
        c += p;
    c *= 1.f / (float)N;

    float rMin = 1e9f, rMax = 0.f;
    for (auto &p : pts)
    {
        float r = cv::norm(p - c);
        rMin = std::min(rMin, r);
        rMax = std::max(rMax, r);
    }
    return (rMin > 1e-6f) ? rMax / rMin : 1.f;
}

// Retângulo rotacionado mínimo: estável mesmo em retas perfeitas ou de ida e volta
float FeatureExtractor::calculateAspectRatio(std::vector<cv::Point2f> &pts)
{
    if (pts.size() < 3)
        return 0.0f;
    cv::RotatedRect box = cv::minAreaRect(pts);
    float major = std::max(box.size.width, box.size.height);
    float minor = std::min(box.size.width, box.size.height);
    return (major > 1e-5f) ? (minor / major) : 0.0f;
}

float FeatureExtractor::calculateWindingAngleDeg(std::vector<cv::Point2f> &pts)
{
    int N = (int)pts.size();
    if (N < 4)
        return 0.0f;

    cv::Point2f c(0.f, 0.f);
    for (auto &p : pts)
        c += p;
    c *= 1.f / (float)N;

    float totalAngle = 0.0f;
    for (int i = 1; i < N; ++i)
    {
        cv::Point2f v1 = pts[i - 1] - c;
        cv::Point2f v2 = pts[i] - c;
        float cross = v1.x * v2.y - v1.y * v2.x;
        float dot = v1.x * v2.x + v1.y * v2.y;
        totalAngle += std::atan2(cross, dot);
    }
    return std::abs(totalAngle) * 180.0f / (float)CV_PI;
}

// Conta apenas curvas laterais reais (entre ~18° e ~146°); retornos de ~180° (ida e volta) não contam como zigue-zague
int FeatureExtractor::countZigZagTurns(std::vector<cv::Point2f> &poly)
{
    if (poly.size() < 3)
        return 0;
    int turns = 0;
    int prevSign = 0;

    for (size_t i = 1; i + 1 < poly.size(); ++i)
    {
        cv::Point2f a = poly[i] - poly[i - 1];
        cv::Point2f b = poly[i + 1] - poly[i];
        float cross = a.x * b.y - a.y * b.x;
        float dot = a.x * b.x + a.y * b.y;
        float angle = std::abs(std::atan2(cross, dot));

        if (angle > 0.32f && angle < 2.55f)
        {
            int s = (cross > 0) - (cross < 0);
            if (prevSign != 0 && s != prevSign)
            {
                turns++;
            }
            prevSign = s;
        }
    }
    return turns;
}

ShapeFeatures FeatureExtractor::extract(std::vector<cv::Point2f> &pts)
{
    ShapeFeatures f;
    int N = (int)pts.size();
    if (N < 8)
        return f;

    // 1. Fechamento e ângulo de varredura
    float gap = cv::norm(pts.front() - pts.back());
    double perOpen = cv::arcLength(pts, false);
    f.endGapRatio = (perOpen > 1e-6) ? (gap / (float)perOpen) : 1.0f;
    f.closed = (f.endGapRatio < 0.24f);
    f.windingAngleDeg = calculateWindingAngleDeg(pts);

    // 2. Área, perímetro e circularidade
    f.area = std::abs(cv::contourArea(pts));
    f.perimeter = cv::arcLength(pts, f.closed);
    f.circularity = (float)calculateCircularity(f.area, f.perimeter);

    std::vector<cv::Point2f> hull;
    cv::convexHull(pts, hull);
    f.hullArea = std::abs(cv::contourArea(hull));
    double hullPerim = cv::arcLength(hull, true);
    f.hullCircularity = (float)calculateCircularity(f.hullArea, hullPerim);
    f.aspectRatio = calculateAspectRatio(pts);

    // 3. Vértices simplificados
    std::vector<cv::Point2f> polyClosed;
    cv::approxPolyDP(pts, polyClosed, 0.038 * f.perimeter, f.closed);
    f.numVertices = (int)polyClosed.size();

    std::vector<cv::Point2f> polyOpen;
    cv::approxPolyDP(pts, polyOpen, 0.030 * perOpen, false);
    f.polyVertices = (int)polyOpen.size();
    f.turns = countZigZagTurns(polyOpen);

    // 4. Ajuste de reta
    cv::Vec4f line;
    cv::fitLine(pts, line, cv::DIST_L2, 0, 0.01, 0.01);
    f.lineAngleDeg = std::atan2(line[1], line[0]) * 180.f / (float)CV_PI;
    cv::Point2f p0(line[2], line[3]);
    cv::Point2f dir(line[0], line[1]);

    for (auto &p : pts)
    {
        cv::Point2f d = p - p0;
        float perp = std::abs(d.x * dir.y - d.y * dir.x);
        f.maxPerpDev = std::max(f.maxPerpDev, perp);
    }

    // 5. Quinas e proporção radial
    f.cornerDefinition = calculateCornerMass(pts, f.closed);
    f.radialRatio = calculateRadialRatio(pts);

    // 6. Alternância de raios e uniformidade das pontas a partir do centro (Estrela)
    cv::Point2f c(0.f, 0.f);
    for (auto &p : pts)
        c += p;
    c *= 1.f / (float)N;

    std::vector<cv::Point2f> polyStar;
    cv::approxPolyDP(pts, polyStar, 0.022 * f.perimeter, f.closed);
    if (polyStar.size() >= 7)
    {
        size_t M = polyStar.size();
        std::vector<float> r(M);
        for (size_t i = 0; i < M; ++i)
            r[i] = cv::norm(polyStar[i] - c);

        int alt = 0;
        for (size_t i = 1; i + 1 < M; ++i)
        {
            if ((r[i] > r[i - 1]) != (r[i + 1] > r[i]))
                ++alt;
        }
        f.altRadii = (float)alt / (float)(M - 2) > 0.65f;

        if (f.altRadii)
        {
            std::vector<float> tipLengths;
            for (size_t i = 0; i < M; ++i)
            {
                size_t prev = (i + M - 1) % M;
                size_t next = (i + 1) % M;
                if (r[i] > r[prev] && r[i] > r[next])
                {
                    tipLengths.push_back(r[i]);
                }
            }
            if (tipLengths.size() >= 4)
            {
                auto mm = std::minmax_element(tipLengths.begin(), tipLengths.end());
                f.starTipRatio = (*mm.second > 1e-6f) ? (*mm.first / *mm.second) : 0.0f;
            }
        }
    }

    return f;
}