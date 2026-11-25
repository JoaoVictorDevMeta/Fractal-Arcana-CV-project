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
    // Dissolve em pixels: o bloco i some quando 'fade' (0..1) passa do seu limiar.
    inline bool visible(int i, int seed, float fade) { return hash01(i, seed) >= fade; }
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

    // Fonte de fliperama 3x5 em blocos: digitos, A-Z (minusculas viram maiusculas) e . , - ! : '
    // Cada glifo e escrito linha a linha (5 linhas de 3 colunas).
    inline const char *pixelGlyph(char c)
    {
        if (c >= 'a' && c <= 'z')
            c = char(c - 'a' + 'A');
        switch (c)
        {
        // clang-format off
        case '0': return "XXX" "X.X" "X.X" "X.X" "XXX";
        case '1': return ".X." "XX." ".X." ".X." "XXX";
        case '2': return "XXX" "..X" "XXX" "X.." "XXX";
        case '3': return "XXX" "..X" ".XX" "..X" "XXX";
        case '4': return "X.X" "X.X" "XXX" "..X" "..X";
        case '5': return "XXX" "X.." "XXX" "..X" "XXX";
        case '6': return "XXX" "X.." "XXX" "X.X" "XXX";
        case '7': return "XXX" "..X" "..X" "..X" "..X";
        case '8': return "XXX" "X.X" "XXX" "X.X" "XXX";
        case '9': return "XXX" "X.X" "XXX" "..X" "XXX";
        case 'A': return ".X." "X.X" "XXX" "X.X" "X.X";
        case 'B': return "XX." "X.X" "XX." "X.X" "XX.";
        case 'C': return ".XX" "X.." "X.." "X.." ".XX";
        case 'D': return "XX." "X.X" "X.X" "X.X" "XX.";
        case 'E': return "XXX" "X.." "XX." "X.." "XXX";
        case 'F': return "XXX" "X.." "XX." "X.." "X..";
        case 'G': return ".XX" "X.." "X.X" "X.X" ".XX";
        case 'H': return "X.X" "X.X" "XXX" "X.X" "X.X";
        case 'I': return "XXX" ".X." ".X." ".X." "XXX";
        case 'J': return "..X" "..X" "..X" "X.X" ".X.";
        case 'K': return "X.X" "X.X" "XX." "X.X" "X.X";
        case 'L': return "X.." "X.." "X.." "X.." "XXX";
        case 'M': return "X.X" "XXX" "XXX" "X.X" "X.X";
        case 'N': return "XX." "X.X" "X.X" "X.X" "X.X";
        case 'O': return ".X." "X.X" "X.X" "X.X" ".X.";
        case 'P': return "XXX" "X.X" "XXX" "X.." "X..";
        case 'Q': return ".X." "X.X" "X.X" "XX." ".XX";
        case 'R': return "XX." "X.X" "XX." "X.X" "X.X";
        case 'S': return ".XX" "X.." ".X." "..X" "XX.";
        case 'T': return "XXX" ".X." ".X." ".X." ".X.";
        case 'U': return "X.X" "X.X" "X.X" "X.X" "XXX";
        case 'V': return "X.X" "X.X" "X.X" "X.X" ".X.";
        case 'W': return "X.X" "X.X" "XXX" "XXX" "X.X";
        case 'X': return "X.X" "X.X" ".X." "X.X" "X.X";
        case 'Y': return "X.X" "X.X" ".X." ".X." ".X.";
        case 'Z': return "XXX" "..X" ".X." "X.." "XXX";
        case '.': return "..." "..." "..." "..." ".X.";
        case ',': return "..." "..." "..." ".X." "X..";
        case '-': return "..." "..." "XXX" "..." "...";
        case '!': return ".X." ".X." ".X." "..." ".X.";
        case ':': return "..." ".X." "..." ".X." "...";
        case '\'': return ".X." ".X." "..." "..." "...";
        default: return "..." "..." "..." "..." "...";
        // clang-format on
        }
    }
    inline int pixelTextWidth(const std::string &s, int px) { return int(s.size()) * 4 * px - px; }
    inline void pixelText(cv::Mat &v, const std::string &s, cv::Point tl, int px, const cv::Scalar &col)
    {
        for (size_t i = 0; i < s.size(); ++i)
        {
            const char *g = pixelGlyph(s[i]);
            for (int k = 0; k < 15; ++k)
                if (g[k] == 'X')
                    cv::rectangle(v, cv::Rect(tl.x + int(i) * 4 * px + (k % 3) * px, tl.y + (k / 3) * px, px, px), col,
                                  cv::FILLED);
        }
    }

    inline cv::Size textSize(const std::string &s, int font, double scale, int thick)
    {
        int base = 0;
        return cv::getTextSize(s, font, scale, thick, &base);
    }
} // namespace pfx
