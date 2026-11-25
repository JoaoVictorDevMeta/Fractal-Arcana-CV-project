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
    const cv::Size ns = textSize(name, cv::FONT_HERSHEY_DUPLEX, big, 2);
    const int textX = side == 0 ? pr.br().x + W / 30 : pr.x - W / 30 - ns.width;
    outlinedText(img, "ESPECIAL!", {textX, top + bh * 28 / 100}, cv::FONT_HERSHEY_DUPLEX, big * 0.5,
                 lighten(accent, 0.3), 1);
    outlinedText(img, name, {textX, cy + ns.height / 2}, cv::FONT_HERSHEY_DUPLEX, big, cv::Scalar(255, 255, 255), 2);
    outlinedText(img, charName, {textX, bot - bh * 14 / 100}, cv::FONT_HERSHEY_SIMPLEX, big * 0.5,
                 cv::Scalar(220, 220, 230), 1);
}
