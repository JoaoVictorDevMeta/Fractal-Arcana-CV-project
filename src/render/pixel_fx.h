#pragma once
#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <string>

// Primitivas de desenho em "blocos de pixel" (grade de px x px), para efeitos no
// mesmo estilo pixel art dos sprites. Tudo inline: usado por varios modulos de render.
namespace pfx
{
    constexpr float kPi = 3.14159265f;

    inline float hash11(int a, int b) // ruido deterministico em [-1, 1]
    {
        unsigned h = unsigned(a) * 374761393u + unsigned(b) * 668265263u;
        h = (h ^ (h >> 13)) * 1274126177u;
        h ^= h >> 16;
        return (h & 0xFFFF) / 65535.f * 2.f - 1.f;
    }
    inline float hash01(int a, int b) { return 0.5f + 0.5f * hash11(a, b); }
    inline float clamp01(float v) { return std::clamp(v, 0.f, 1.f); }
    inline float seg(float t, float a, float b) { return clamp01((t - a) / (b - a)); }
    inline float easeOut(float u)
    {
        u = clamp01(u);
        return 1.f - (1.f - u) * (1.f - u);
    }
    inline float stepT(float t, float fps = 15.f) { return std::floor(t * fps) / fps; }

    inline cv::Scalar lighten(const cv::Scalar &c, double k)
    {
        return cv::Scalar(c[0] + (255 - c[0]) * k, c[1] + (255 - c[1]) * k, c[2] + (255 - c[2]) * k);
    }

    inline void pix(cv::Mat &v, float x, float y, int px, const cv::Scalar &c, int mult = 1)
    {
        const int gx = int(std::floor(x / px)) * px;
        const int gy = int(std::floor(y / px)) * px;
        cv::rectangle(v, cv::Rect(gx, gy, px * mult, px * mult), c, cv::FILLED);
    }

    inline void pixLine(cv::Mat &v, cv::Point2f a, cv::Point2f b, int px, const cv::Scalar &c)
    {
        const float len = std::hypot(b.x - a.x, b.y - a.y);
        const int n = std::max(1, int(len / px));
        for (int i = 0; i <= n; ++i)
        {
            const float u = float(i) / n;
            pix(v, a.x + (b.x - a.x) * u, a.y + (b.y - a.y) * u, px, c);
        }
    }

    // Disco cheio de blocos; 'shade' clareia o lado de cima-esquerda (volume).
    inline void pixDisc(cv::Mat &v, cv::Point2f c, float r, int px, const cv::Scalar &col, bool shade = true)
    {
        for (float y = -r; y <= r; y += px)
            for (float x = -r; x <= r; x += px)
            {
                const float d = std::hypot(x, y);
                if (d > r)
                    continue;
                cv::Scalar k = col;
                if (shade)
                {
                    const float l = (-x - y) / (2.f * r); // -0.7..0.7
                    k = l > 0.25f ? lighten(col, 0.45) : (l < -0.35f ? col * 0.6 : col);
                    if (d > r - px)
                        k = col * 0.45; // contorno
                }
                pix(v, c.x + x, c.y + y, px, k);
            }
    }

    inline void pixRing(cv::Mat &v, cv::Point2f c, float r, float ry, int px, const cv::Scalar &col, float spin = 0.f,
                        int gapEvery = 0)
    {
        if (r <= 0.f)
            return;
        const int n = std::max(12, int(2.f * kPi * r / (px * 1.4f)));
        for (int i = 0; i < n; ++i)
        {
            if (gapEvery > 0 && i % gapEvery == 0)
                continue;
            const float a = 2.f * kPi * i / n + spin;
            pix(v, c.x + std::cos(a) * r, c.y + std::sin(a) * r * ry, px, col);
        }
    }

    // Texto com contorno escuro grosso (legivel sobre qualquer fundo).
    inline void outlinedText(cv::Mat &img, const std::string &s, cv::Point org, int font, double scale,
                             const cv::Scalar &fg, int thick, const cv::Scalar &outline = cv::Scalar(15, 10, 20))
    {
        cv::putText(img, s, org, font, scale, outline, thick + 4, cv::LINE_AA);
        cv::putText(img, s, org, font, scale, fg, thick, cv::LINE_AA);
    }

    inline cv::Size textSize(const std::string &s, int font, double scale, int thick)
    {
        int base = 0;
        return cv::getTextSize(s, font, scale, thick, &base);
    }
} // namespace pfx
