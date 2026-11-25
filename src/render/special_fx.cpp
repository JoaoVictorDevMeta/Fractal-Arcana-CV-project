#include "render/special_fx.h"
#include "render/pixel_fx.h"
#include "render/sprites.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>

using namespace pfx;

cv::Scalar specialColor(SpecialKind k)
{
    switch (k)
    {
    case SpecialKind::ChuvaDeCodigo:
        return cv::Scalar(90, 230, 90); // verde terminal
    case SpecialKind::OlhoQueTudoVe:
        return cv::Scalar(70, 200, 245); // dourado
    case SpecialKind::CargaDoGeneral:
        return cv::Scalar(230, 120, 60); // azul da farda
    case SpecialKind::SobrecargaSistema:
        return cv::Scalar(255, 210, 80); // ciano
    case SpecialKind::FuriaDaFloresta:
        return cv::Scalar(80, 220, 90); // verde folha
    case SpecialKind::SingularidadeMana:
        return cv::Scalar(255, 235, 140); // ciano claro
    default:
        return cv::Scalar(230, 90, 230); // magenta
    }
}

namespace
{
    void eyeGlyph(cv::Mat &v, cv::Point2f c, float s, int px, const cv::Scalar &gold)
    {
        const cv::Point2f top(c.x, c.y - s), bl(c.x - s, c.y + s * 0.75f), br(c.x + s, c.y + s * 0.75f);
        // triangulo preenchido com borda clara
        for (float y = -s; y <= s * 0.75f; y += px)
        {
            const float half = s * (y + s) / (1.75f * s);
            for (float x = -half; x <= half; x += px)
                pix(v, c.x + x, c.y + y, px, gold * 0.55);
        }
        pixLine(v, top, bl, px, lighten(gold, 0.4));
        pixLine(v, top, br, px, lighten(gold, 0.4));
        pixLine(v, bl, br, px, lighten(gold, 0.4));
        const cv::Scalar white(255, 255, 255), iris(70, 40, 20);
        const float ew = s * 0.48f, eh = s * 0.2f, ey = c.y + s * 0.18f;
        for (float x = -ew; x <= ew; x += px)
        {
            const float yy = eh * std::sqrt(std::max(0.f, 1.f - (x * x) / (ew * ew)));
            for (float y = -yy; y <= yy; y += px)
                pix(v, c.x + x, ey + y, px, std::hypot(x, y) < eh * 0.85f ? iris : white);
        }
        pix(v, c.x - px, ey - px, px, white);
    }
} // namespace

void drawSpecialProjectile(cv::Mat &view, SpecialKind k, cv::Point center, int dir, float time, int H)
{
    const int px = std::max(2, H / 54);
    const float s = H * 0.22f;
    const cv::Point2f c(float(center.x), float(center.y));
    const float tq = stepT(time, 15.f);
    const int frame = int(time * 15.f);
    const cv::Scalar white(255, 255, 255);
    const cv::Scalar col = specialColor(k);

    switch (k)
    {
    case SpecialKind::OlhoQueTudoVe: // triangulo com o olho, raios girando
    {
        for (int i = 0; i < 10; ++i)
        {
            const float a = i * 2.f * kPi / 10.f + tq * 3.f;
            const float r0 = s * 0.9f, r1 = s * (i % 2 ? 1.25f : 1.55f);
            pixLine(view, {c.x + std::cos(a) * r0, c.y + std::sin(a) * r0},
                    {c.x + std::cos(a) * r1, c.y + std::sin(a) * r1}, px, i % 2 ? col : lighten(col, 0.5));
        }
        eyeGlyph(view, c, s * 0.85f, px, col);
        break;
    }
    case SpecialKind::CargaDoGeneral: // lamina crescente azul com fio branco e rastro vermelho
    {
        const float r = s * 0.9f;
        for (int i = 1; i <= 5; ++i) // rastro
            pix(view, c.x - dir * (r * 0.4f + i * px * 2.5f), c.y + hash11(i, frame) * r * 0.5f, px,
                i % 2 ? cv::Scalar(50, 50, 220) : cv::Scalar(230, 150, 90), i < 3 ? 2 : 1);
        for (float y = -r; y <= r; y += px)
            for (float x = -r; x <= r; x += px)
            {
                const float d1 = std::hypot(x, y);
                const float d2 = std::hypot(x + dir * 0.45f * r, y);
                if (d1 > r || d2 < r * 0.9f)
                    continue;
                const float edge = r - d1; // perto do fio da lamina
                const cv::Scalar k2 = edge < px * 1.5f ? white : (d2 < r * 1.05f ? cv::Scalar(50, 50, 220) : col);
                pix(view, c.x + x, c.y + y, px, k2);
            }
        break;
    }
    case SpecialKind::SobrecargaSistema: // celular com apps + ondas de sinal + falhas
    {
        const float w = s * 0.95f, h = s * 1.5f;
        const cv::Point2f tl(c.x - w / 2, c.y - h / 2);
        for (float y = 0; y < h; y += px)
            for (float x = 0; x < w; x += px)
            {
                const bool border = x < px * 1.5f || y < px * 2.5f || x > w - px * 2.5f || y > h - px * 3.5f;
                pix(view, tl.x + x, tl.y + y, px, border ? cv::Scalar(45, 35, 35) : cv::Scalar(120, 70, 30));
            }
        const cv::Scalar apps[4] = {{60, 200, 255}, {230, 90, 200}, {90, 220, 90}, {255, 200, 60}};
        const float cell = (w - px * 5) / 3.f;
        for (int r = 0; r < 3; ++r)
            for (int q = 0; q < 3; ++q)
                if (hash01(r * 3 + q, frame / 2) > 0.15f)
                    cv::rectangle(view, cv::Rect(int(tl.x + px * 2 + q * cell + px / 2), int(tl.y + px * 4 + r * cell),
                                                 std::max(px, int(cell - px)), std::max(px, int(cell - px))),
                                  apps[(r + q + frame / 3) % 4], cv::FILLED);
        for (int a = 0; a < 3; ++a) // sinal saindo pela frente
        {
            const float rr = s * (0.9f + 0.3f * a) + std::fmod(tq * 40.f, float(px * 3));
            for (float ang = -0.6f; ang <= 0.6f; ang += 0.12f)
                pix(view, c.x + dir * std::cos(ang) * rr, c.y + std::sin(ang) * rr, px, lighten(col, 0.3 * a));
        }
        for (int i = 0; i < 5; ++i) // falhas
            if (hash01(i, frame) > 0.4f)
                cv::rectangle(view, cv::Rect(int(c.x + hash11(i, frame + 3) * s * 1.3f), int(c.y + hash11(i, frame + 5) * s),
                                             px * (2 + i), px), apps[i % 4], cv::FILLED);
        break;
    }
    case SpecialKind::FuriaDaFloresta: // semente verde com trepadeiras e folhas
    {
        const float r = s * 0.7f;
        for (int i = 0; i < 7; ++i) // folhas no rastro
        {
            const float back = r + (i + 1) * px * 3.f;
            pix(view, c.x - dir * back, c.y + std::sin(tq * 10.f + i) * r * 0.6f, px,
                i % 2 ? cv::Scalar(90, 235, 120) : cv::Scalar(40, 140, 60), 2);
        }
        pixDisc(view, c, r, px, cv::Scalar(60, 190, 70));
        for (int v = 0; v < 3; ++v) // trepadeiras em espiral
            for (int j = 0; j < 12; ++j)
            {
                const float a = v * 2.f * kPi / 3.f + j * 0.35f + tq * 5.f;
                const float rr = r * (0.9f + 0.04f * j);
                pix(view, c.x + std::cos(a) * rr, c.y + std::sin(a) * rr, px, j % 4 == 3 ? cv::Scalar(120, 255, 150)
                                                                                         : cv::Scalar(30, 110, 40));
            }
        pix(view, c.x - r * 0.3f, c.y - r * 0.35f, px, white, 2); // brilho
        break;
    }
    case SpecialKind::SingularidadeMana: // nucleo escuro, aneis de mana girando
    {
        for (int i = 0; i < 16; ++i) // particulas sendo sugadas
        {
            const float ph = std::fmod(tq * 1.8f + hash01(i, 7), 1.f);
            const float a = i * 2.f * kPi / 16.f + ph * 3.f;
            const float rr = s * (1.5f - 1.1f * ph);
            pix(view, c.x + std::cos(a) * rr, c.y + std::sin(a) * rr, px, i % 3 ? col : white);
        }
        pixRing(view, c, s * 0.95f, 0.35f, px, col, tq * 4.f, 5);
        pixRing(view, c, s * 0.75f, 1.f, px, lighten(col, 0.4), -tq * 3.f, 4);
        pixDisc(view, c, s * 0.45f, px, cv::Scalar(60, 20, 45), false);
        pixRing(view, c, s * 0.45f, 1.f, px, white, 0.f);
        break;
    }
    case SpecialKind::ChuvaDeCodigo: // bloco de codigo (so sem o sprite do Derzu)
    {
        const float w = s * 1.6f, h = s * 1.2f;
        cv::rectangle(view, cv::Rect(int(c.x - w / 2), int(c.y - h / 2), int(w), int(h)), cv::Scalar(30, 25, 20), cv::FILLED);
        for (int r = 0; r < 6; ++r)
        {
            const float len = (0.3f + 0.6f * hash01(r, frame / 4)) * (w - px * 4);
            const float x0 = c.x - w / 2 + px * 2 + (r % 3) * px * 2;
            pixLine(view, {x0, c.y - h / 2 + px * 2 + r * (h - px * 4) / 6.f},
                    {x0 + len, c.y - h / 2 + px * 2 + r * (h - px * 4) / 6.f}, px, r % 2 ? col : cv::Scalar(230, 90, 200));
        }
        break;
    }
    default:
        pixDisc(view, c, s * 0.7f, px, col);
        break;
    }
}

// ---------------------------------------------------------------------------
// Baltar: Furia de Maxwell
// ---------------------------------------------------------------------------
void drawMaxwellTiger(cv::Mat &view, const std::vector<cv::Mat> &frames, int dir, float t,
                      int xFrom, int xTo, int H, float time)
{
    if (frames.size() < 6 || H <= 8)
        return;
    const int px = std::max(2, H / 54);
    const int dist = std::abs(xTo - xFrom);
    const int spawn = xFrom + dir * int(dist * 0.14f);
    const int strike = xTo - dir * int(H * 0.45f);

    int idx = 0, x = spawn;
    if (t < 0.30f) // gato -> explosao verde -> tigre surge na fumaca
        idx = t < 0.10f ? 0 : (t < 0.20f ? 1 : 2);
    else if (t < 0.88f) // corre ate o alvo alternando rugido e salto
    {
        const float u = seg(t, 0.30f, 0.88f);
        x = spawn + int((strike - spawn) * u);
        idx = (int(time * 8.f) % 2) ? 4 : 3;
        for (int i = 0; i < 6; ++i) // poeira e folhas atras
            pix(view, x - dir * (H * 0.35f + i * px * 3), H - 4 - px - hash01(i, int(time * 15)) * H * 0.12f, px,
                i % 2 ? cv::Scalar(150, 170, 160) : cv::Scalar(60, 170, 70), 2);
    }
    else // garra
    {
        idx = 5;
        x = strike + dir * int(H * 0.25f);
    }

    cv::Mat f = frames[idx];
    if (f.rows > H - 4) // arena baixa: reduz o quadro
    {
        const double s = double(H - 4) / f.rows;
        cv::resize(f, f, {}, s, s, cv::INTER_NEAREST);
    }
    drawSprite(view, f, {x, H - 4 - f.rows / 2});
}

// ---------------------------------------------------------------------------
// Neri: Jesus Cristo!
// ---------------------------------------------------------------------------
namespace
{
    // cruzinha de pixels (as douradas e azuis que flutuam em volta)
    void miniCross(cv::Mat &v, float x, float y, int px, const cv::Scalar &col)
    {
        pix(v, x, y - 2 * px, px, col);
        pix(v, x, y - px, px, col);
        pix(v, x - px, y - px, px, col);
        pix(v, x + px, y - px, px, col);
        pix(v, x, y, px, col);
        pix(v, x, y + px, px, col);
    }

    // simbolo de wi-fi (arcos virados para cima)
    void wifi(cv::Mat &v, float x, float y, int px, const cv::Scalar &col)
    {
        pix(v, x, y, px, col);
        for (int a = 1; a <= 3; ++a)
            for (float ang = -2.4f; ang <= -0.7f; ang += 0.35f / a)
                pix(v, x + std::cos(ang) * a * px * 2.f, y + std::sin(ang) * a * px * 2.f, px, col);
    }

    // barra de luz em camadas: azul por fora, ciano, nucleo branco e filete dourado
    void lightBar(cv::Mat &v, const cv::Rect &r, int px, float flash)
    {
        const cv::Scalar deep(230, 110, 30), cyan(255, 225, 130), white(255, 255, 255), gold(60, 200, 250);
        auto shrink = [](cv::Rect a, int d) { return cv::Rect(a.x + d, a.y + d, std::max(1, a.width - 2 * d), std::max(1, a.height - 2 * d)); };
        cv::rectangle(v, cv::Rect(r.x - px, r.y - px, r.width + 2 * px, r.height + 2 * px), deep, cv::FILLED);
        cv::rectangle(v, r, flash > 0.5f ? white : cyan, cv::FILLED);
        const int d = std::max(px, std::min(r.width, r.height) / 4);
        cv::rectangle(v, shrink(r, d), white, cv::FILLED);
        if (std::min(r.width, r.height) > 6 * px)
            cv::rectangle(v, shrink(r, d / 2), gold, std::max(1, px / 2));
    }
} // namespace

void drawHolyCross(cv::Mat &view, int xTarget, float t, int H, float time)
{
    const int px = std::max(2, H / 60);
    const int ground = H - 4;
    const float tq = stepT(t * 1.3f, 15.f) / 1.3f; // em "quadros" de 15 qps
    const cv::Scalar white(255, 255, 255), cyan(255, 225, 130), gold(60, 200, 250);
    const int frame = int(time * 15.f);
    const float cx = float(xTarget);
    const float armY = H * 0.36f;

    // A) feixe de luz descendo do ceu
    if (tq < 0.25f)
    {
        const float a = easeOut(seg(tq, 0.f, 0.2f));
        const int w = std::max(px, int(H * 0.05f));
        lightBar(view, cv::Rect(int(cx) - w / 2, 0, w, std::max(1, int(ground * a))), px, 0.f);
    }

    // B/C) a cruz cresce e brilha
    const float b = easeOut(seg(tq, 0.18f, 0.70f));
    const float flash = (tq > 0.70f && tq < 0.85f) ? 1.f : 0.f;
    if (tq >= 0.18f && tq < 0.85f)
    {
        const int vw = std::max(px, int((0.035f + 0.045f * b) * H));
        const int hl = int(b * 0.55f * H);
        const int hh = std::max(px, int(vw * 0.9f));
        lightBar(view, cv::Rect(int(cx) - vw, 0, 2 * vw, ground), px, flash);
        if (hl > vw)
            lightBar(view, cv::Rect(int(cx) - hl, int(armY) - hh, 2 * hl, 2 * hh), px, flash);
        // estrela no cruzamento
        for (int r = 0; r < 8; ++r)
        {
            const float ang = r * kPi / 4.f + tq * 2.f;
            const float len = (0.15f + 0.25f * b) * H * (r % 2 ? 0.6f : 1.f);
            pixLine(view, {cx, armY}, {cx + std::cos(ang) * len, armY + std::sin(ang) * len}, px, r % 2 ? gold : white);
        }
        // cruzinhas e sinais de wi-fi flutuando
        for (int i = 0; i < 10; ++i)
        {
            const float ph = std::fmod(time * 0.6f + hash01(i, 3), 1.f);
            const float x = cx + hash11(i, 5) * H * 0.75f;
            const float y = ground - ph * H * 0.9f;
            if (i % 4 == 3)
                wifi(view, x, y, std::max(1, px / 2), cyan);
            else
                miniCross(view, x, y, std::max(1, px * 2 / 3), i % 2 ? gold : cyan);
        }
    }

    // D) a cruz se desfaz em pixels que voam para fora
    if (tq >= 0.85f)
    {
        const float u = seg(tq, 0.85f, 1.f);
        const int vw = int(0.08f * H), hl = int(0.55f * H), hh = int(vw * 0.9f);
        int id = 0;
        for (int y = 0; y < ground; y += px * 2)
            for (int x = -vw; x < vw; x += px * 2, ++id)
                if (visible(id, frame, u))
                    pix(view, cx + x + hash11(id, 1) * u * H * 0.5f, y + hash11(id, 2) * u * H * 0.2f, px,
                        id % 3 ? cyan : white);
        for (int x = -hl; x < hl; x += px * 2)
            for (int y = -hh; y < hh; y += px * 2, ++id)
                if (visible(id, frame, u))
                    pix(view, cx + x + hash11(id, 1) * u * H * 0.5f, armY + y + hash11(id, 2) * u * H * 0.3f, px,
                        id % 3 ? cyan : gold);
    }
}

// ---------------------------------------------------------------------------
// Venegas: La integral es la soma de las partes!
// ---------------------------------------------------------------------------
namespace
{
    cv::Scalar mix(const cv::Scalar &a, const cv::Scalar &b, double u)
    {
        return cv::Scalar(a[0] + (b[0] - a[0]) * u, a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u);
    }

    // degrade das barras: azul -> roxo -> branco -> vermelho (as cores da faixa do Venegas)
    cv::Scalar barColor(float u)
    {
        const cv::Scalar c[4] = {{210, 90, 40}, {200, 60, 150}, {235, 225, 240}, {60, 60, 220}};
        u = clamp01(u) * 3.f;
        const int i = std::min(2, int(u));
        return mix(c[i], c[i + 1], u - i);
    }

    void glyph(cv::Mat &v, const char *const *rows, int nRows, float x, float y, int px, const cv::Scalar &col)
    {
        for (int r = 0; r < nRows; ++r)
            for (int c = 0; rows[r][c]; ++c)
                if (rows[r][c] == 'X')
                {
                    pix(v, x + c * px + px / 2, y + r * px + px / 2, px, cv::Scalar(20, 40, 60)); // sombra
                    pix(v, x + c * px, y + r * px, px, col);
                }
    }
} // namespace

void drawIntegralWave(cv::Mat &view, int xFrom, int xTo, float front01, int H, float time)
{
    if (front01 <= 0.f)
        return;
    const int dir = xTo >= xFrom ? 1 : -1;
    const int px = std::max(2, H / 60);
    const int ground = H - 4;
    const float start = float(xFrom + dir * int(H * 0.28f)); // sai da mao do Venegas
    const float end = float(xTo - dir * int(H * 0.15f));
    const float L = std::abs(end - start);
    if (L < 4.f)
        return;
    front01 = clamp01(front01);
    const float reach = L * front01;
    auto curveY = [&](float d) // d = distancia percorrida a partir de 'start'
    {
        const float u = d / L;
        const float y = ground - H * (0.30f + 0.20f * std::sin(u * 2.f * kPi * 1.25f - 0.6f) + 0.22f * u);
        return std::max(H * 0.08f, y);
    };

    // barras da soma de Riemann (as "partes")
    const int N = 8;
    const float bw = L / N;
    for (int i = 0; i < N; ++i)
    {
        const float d0 = i * bw;
        if (d0 >= reach)
            break;
        const float w = std::min(bw, reach - d0) - px;
        if (w < px)
            continue;
        const float top = curveY(d0 + bw * 0.5f);
        const cv::Scalar col = barColor(float(i) / (N - 1));
        const int xa = int(start + dir * d0), xb = int(start + dir * (d0 + w));
        const cv::Rect r(std::min(xa, xb), int(top), std::abs(xb - xa), std::max(1, ground - int(top)));
        cv::rectangle(view, r, col * 0.55, cv::FILLED);
        cv::rectangle(view, cv::Rect(r.x + px, r.y + px, std::max(1, r.width / 3), std::max(1, r.height - px)), col,
                      cv::FILLED); // reflexo
        cv::rectangle(view, cv::Rect(r.x, r.y, r.width, px), cv::Scalar(255, 255, 255), cv::FILLED);
        for (int s = 0; s < 4; ++s) // brilhos dentro da barra
            pix(view, r.x + hash01(i * 7 + s, int(time * 10)) * r.width, r.y + hash01(i * 5 + s, int(time * 10) + 1) * r.height,
                px, lighten(col, 0.6));
    }

    // a curva f(x), ate a frente da onda
    cv::Point2f prev(start, curveY(0.f));
    for (float d = float(px); d <= reach; d += px)
    {
        const cv::Point2f cur(start + dir * d, curveY(d));
        pixLine(view, prev + cv::Point2f(0.f, float(px)), cur + cv::Point2f(0.f, float(px)), px, cv::Scalar(255, 160, 50));
        pixLine(view, prev, cur, px, cv::Scalar(255, 245, 200));
        prev = cur;
    }

    // centelhas na ponta e simbolos dourados acima
    const cv::Point2f tip(start + dir * reach, curveY(reach));
    for (int i = 0; i < 8; ++i)
        pix(view, tip.x + hash11(i, int(time * 15)) * H * 0.08f, tip.y + hash11(i + 9, int(time * 15)) * H * 0.08f, px,
            i % 2 ? cv::Scalar(255, 255, 255) : cv::Scalar(255, 210, 120));
    static const char *sigma[7] = {"XXXXX", "X....", ".X...", "..X..", ".X...", "X....", "XXXXX"};
    static const char *integ[9] = {"..XX", ".X..", ".X..", ".X..", ".X..", ".X..", ".X..", ".X..", "XX.."};
    const cv::Scalar gold(60, 200, 250);
    const int gp = std::max(2, px);
    const float gy = std::max(2.f, tip.y - H * 0.30f) + std::sin(time * 6.f) * px;
    glyph(view, sigma, 7, tip.x - dir * H * 0.25f, gy, gp, gold);
    glyph(view, integ, 9, tip.x - dir * H * 0.25f + 7 * gp, gy - gp, gp, gold);
    cv::putText(view, "f(x)dx", {int(tip.x - dir * H * 0.25f + 12 * gp), int(gy + 6 * gp)}, cv::FONT_HERSHEY_SIMPLEX,
                std::max(0.35, H / 400.0), gold, 1, cv::LINE_AA);
}

// ---------------------------------------------------------------------------
// Derzu: bug na tela acompanhando o poder
// ---------------------------------------------------------------------------
void drawScreenBug(cv::Mat &img, int cx, float time, int seed)
{
    if (img.empty())
        return;
    const int W = img.cols, H = img.rows;
    const int bw = std::max(40, W * 9 / 100);
    const cv::Rect band = cv::Rect(cx - bw, 0, 2 * bw, H) & cv::Rect(0, 0, W, H);
    if (band.width < 8)
        return;
    const int frame = int(time * 20.f) + seed * 1000;

    // 1) cores separadas: azul para a esquerda, vermelho para a direita
    {
        cv::Mat roi = img(band);
        std::vector<cv::Mat> ch;
        cv::split(roi, ch);
        const int s = 3 + int(hash01(frame, 1) * 6);
        if (s < band.width)
        {
            for (int c : {0, 2})
            {
                cv::Mat shifted = ch[c].clone();
                const int w = band.width - s;
                if (c == 0)
                    ch[c](cv::Rect(s, 0, w, H)).copyTo(shifted(cv::Rect(0, 0, w, H)));
                else
                    ch[c](cv::Rect(0, 0, w, H)).copyTo(shifted(cv::Rect(s, 0, w, H)));
                ch[c] = shifted;
            }
            cv::merge(ch, roi);
        }
    }

    // 2) linhas rasgadas: faixas horizontais deslocadas
    for (int i = 0; i < 9; ++i)
    {
        if (hash01(i, frame) < 0.35f)
            continue;
        const int y = int(hash01(i, frame + 1) * (H - 4));
        const int h = 3 + int(hash01(i, frame + 2) * 16);
        const int dx = int(hash11(i, frame + 3) * 40);
        const cv::Rect src = cv::Rect(band.x, y, band.width, h) & cv::Rect(0, 0, W, H);
        const cv::Rect dst = cv::Rect(src.x + dx, src.y, src.width, src.height) & cv::Rect(0, 0, W, H);
        if (src.empty() || dst.empty())
            continue;
        const cv::Mat piece = img(src).clone();
        piece(cv::Rect(dst.x - (src.x + dx), 0, dst.width, dst.height)).copyTo(img(dst));
    }

    // 3) blocos corrompidos
    const cv::Scalar bad[4] = {{255, 0, 255}, {0, 255, 0}, {0, 0, 0}, {255, 255, 0}};
    for (int i = 0; i < 12; ++i)
    {
        if (hash01(i, frame + 7) < 0.4f)
            continue;
        const int x = cx + int(hash11(i, frame + 8) * bw * 1.3f);
        const int y = int(hash01(i, frame + 9) * H);
        const int w = 6 + int(hash01(i, frame + 10) * 40), h = 3 + int(hash01(i, frame + 11) * 10);
        cv::rectangle(img, cv::Rect(x, y, w, h), bad[i % 4], cv::FILLED);
    }

    // 4) linhas de varredura escuras
    for (int y = int(time * 60.f) % 3; y < H; y += 3)
        cv::line(img, {band.x, y}, {band.br().x, y}, cv::Scalar(0, 0, 0), 1);

    // 5) mensagens de erro
    static const char *msgs[] = {"0xDEADBEEF", "Segmentation fault", "core dumped", "NullPointerException",
                                 "undefined behavior", "ERRO 404", "stack overflow", "int main() {"};
    for (int i = 0; i < 2; ++i)
    {
        const char *m = msgs[int(hash01(i, frame / 3) * 7.99f)];
        const int x = cx - bw + int(hash01(i + 4, frame / 3) * bw);
        const int y = 12 + int(hash01(i + 6, frame / 3) * (H - 24));
        cv::putText(img, m, {x + 1, y + 1}, cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(0, 0, 0), 2);
        cv::putText(img, m, {x, y}, cv::FONT_HERSHEY_PLAIN, 1.0, cv::Scalar(90, 255, 90), 1);
    }
}

void drawSuperCutIn(cv::Mat &img, int side, float t, const cv::Mat &portrait,
                    const std::string &charName, SpecialKind k)
{
    if (img.empty() || t < 0.f || t > 0.92f)
        return;
    const int W = img.cols, H = img.rows;
    const float tq = stepT(t, 30.f);
    const int dirIn = side == 0 ? 1 : -1; // P1 entra pela esquerda, P2 pela direita
    const cv::Scalar accent = specialColor(k);

    float off = 0.f; // deslocamento horizontal em fracoes da largura
    if (tq < 0.12f)
        off = -dirIn * (1.f - easeOut(tq / 0.12f));
    else if (tq > 0.76f)
    {
        const float u = seg(tq, 0.76f, 0.92f);
        off = dirIn * u * u;
    }
    const int dx = cvRound(off * W);

    // escurece a tela inteira enquanto a faixa esta parada
    if (tq < 0.8f)
        img.convertTo(img, -1, 0.6);

    const int bh = std::max(60, H * 30 / 100);
    const int cy = H * 47 / 100;
    const int top = cy - bh / 2, bot = cy + bh / 2;
    const int skew = bh / 3 * dirIn;

    auto band = [&](int t0, int t1, const cv::Scalar &col)
    {
        const cv::Point poly[4] = {{-W / 4 + dx + skew, t0}, {W + W / 4 + dx + skew, t0},
                                   {W + W / 4 + dx - skew, t1}, {-W / 4 + dx - skew, t1}};
        cv::fillConvexPoly(img, poly, 4, col);
    };
    band(top - 8, bot + 8, accent);
    band(top - 3, bot + 3, cv::Scalar(255, 255, 255));
    band(top, bot, cv::Scalar(18, 12, 26));

    // linhas de velocidade correndo dentro da faixa
    const int lineH = std::max(2, bh / 40);
    for (int i = 0; i < 16; ++i)
    {
        const int y = top + 4 + int(hash01(i, 3) * (bh - 8));
        const float run = std::fmod(t * 2.6f + hash01(i, 5), 1.f);
        const int len = W / 6 + int(hash01(i, 9) * W / 5);
        const int x = side == 0 ? W - int(run * (W + len)) : int(run * (W + len)) - len;
        cv::rectangle(img, cv::Rect(x + dx, y, len, lineH), accent * (0.35 + 0.3 * hash01(i, 11)), cv::FILLED);
    }

    // retrato do personagem, com moldura na cor do especial
    const int ph = bh - 12, pw = cvRound(ph * 1.15);
    const int px0 = (side == 0 ? W * 8 / 100 : W - W * 8 / 100 - pw) + dx;
    const cv::Rect pr(px0, top + 6, pw, ph);
    if (!portrait.empty())
    {
        const cv::Mat face = coverCrop(portrait, pr.size(), 0.3f);
        const cv::Rect vis = pr & cv::Rect(0, 0, W, H);
        if (!vis.empty() && !face.empty() && face.channels() == img.channels())
            face(cv::Rect(vis.x - pr.x, vis.y - pr.y, vis.width, vis.height)).copyTo(img(vis));
    }
    cv::rectangle(img, pr, accent, 4);

    // textos: rotulo, nome do golpe e personagem
    const SpecialDef &sd = specialDef(k);
    const double big = std::clamp(bh / 95.0, 0.7, 1.8);
    const std::string name = sd.name;
    // nomes longos ("La integral es la soma de las partes!") encolhem para caber ao lado do retrato
    const int availW = side == 0 ? W - pr.br().x - W / 30 - W / 25 : pr.x - W / 30 - W / 25;
    cv::Size ns = textSize(name, cv::FONT_HERSHEY_DUPLEX, big, 2);
    const double nameScale = (ns.width > availW && ns.width > 0) ? std::max(0.4, big * availW / ns.width) : big;
    ns = textSize(name, cv::FONT_HERSHEY_DUPLEX, nameScale, 2);
    const int textX = side == 0 ? pr.br().x + W / 30 : pr.x - W / 30 - ns.width;
    outlinedText(img, "ESPECIAL!", {textX, top + bh * 28 / 100}, cv::FONT_HERSHEY_DUPLEX, big * 0.5,
                 lighten(accent, 0.3), 1);
    outlinedText(img, name, {textX, cy + ns.height / 2}, cv::FONT_HERSHEY_DUPLEX, nameScale, cv::Scalar(255, 255, 255), 2);
    outlinedText(img, charName, {textX, bot - bh * 14 / 100}, cv::FONT_HERSHEY_SIMPLEX, big * 0.5,
                 cv::Scalar(220, 220, 230), 1);
}
