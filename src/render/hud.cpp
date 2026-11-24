#include "render/hud.h"
#include "render/sprites.h"
#include "game/match.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// Reduz o sprite se ele for mais alto que a arena (em vez de cortar).
static void drawFit(cv::Mat &view, const cv::Mat &spr, cv::Point center)
{
    const int maxH = view.rows - 4;
    if (maxH > 0 && spr.rows > maxH)
    {
        cv::Mat r;
        const double s = double(maxH) / spr.rows;
        cv::resize(spr, r, {}, s, s, cv::INTER_AREA);
        drawSprite(view, r, center);
    }
    else
        drawSprite(view, spr, center);
}

static float rnd(int a, int b) // ruido deterministico em [-1, 1]
{
    unsigned h = unsigned(a) * 374761393u + unsigned(b) * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFF) / 65535.f * 2.f - 1.f;
}

// Copia do sprite misturada com vermelho (preserva o alfa). k01: 0 = original, 1 = mais vermelho.
static cv::Mat tintRed(const cv::Mat &spr, float k01)
{
    cv::Mat out = spr.clone();
    if (out.empty() || out.depth() != CV_8U || out.channels() < 3)
        return out;

    const float k = std::clamp(k01, 0.f, 1.f) * 0.65f;
    const int cn = out.channels();
    for (int y = 0; y < out.rows; ++y)
    {
        uchar *row = out.ptr<uchar>(y);
        for (int x = 0; x < out.cols; ++x)
        {
            uchar *px = row + x * cn;
            px[0] = uchar(px[0] * (1.f - k) + 40 * k);  // B
            px[1] = uchar(px[1] * (1.f - k) + 40 * k);  // G
            px[2] = uchar(px[2] * (1.f - k) + 255 * k); // R
        }
    }
    return out;
}

// Rastro de raio atras do projetil: head = ponta, dir = +1 (P1) ou -1 (P2).
static void drawLightningTrail(cv::Mat &view, cv::Point tail, cv::Point head,
                               const cv::Scalar &col, float time, int seed, bool glowEffects)
{
    const int dir = (head.x >= tail.x) ? 1 : -1;
    const int len = std::abs(head.x - tail.x);
    const int step = 10;
    const int n = len / step;
    if (n < 2)
        return;

    const int tick = int(time * 24); // o raio "tremula" 24x por segundo

    // pontos a partir do mago; zigue-zague maior no meio, zero nas pontas
    std::vector<cv::Point> pts;
    for (int j = 0; j <= n; ++j)
    {
        float f = float(j) / n; // 0 = mago, 1 = projetil
        float amp = 3.f + 16.f * std::sin(f * 3.14159f);
        int x = (j == n) ? head.x : tail.x + dir * j * step;
        int y = tail.y + int(rnd(j + seed * 97, tick) * amp);
        if (j == 0)
            y = tail.y;
        if (j == n)
            y = head.y;
        pts.push_back({x, y});
    }

    cv::Rect roi(std::min(tail.x, head.x) - 25, tail.y - 60, len + 50, 120);
    roi &= cv::Rect(0, 0, view.cols, view.rows);
    if (roi.empty())
        return;
    const cv::Point off = roi.tl();

    cv::Mat sub = view(roi);

    // brilho (camada aditiva desfocada) - pulado quando os brilhos estao desligados
    if (glowEffects)
    {
        cv::Mat glow = cv::Mat::zeros(roi.size(), view.type());
        for (int j = 0; j < n; ++j)
        {
            double b = 0.55 + 0.45 * double(j) / n; // mais forte perto do projetil
            cv::line(glow, pts[j] - off, pts[j + 1] - off, col * b, 9, cv::LINE_AA);
        }
        cv::GaussianBlur(glow, glow, {13, 13}, 0);
        cv::add(sub, glow, sub);
    }

    // nucleo + ramificacoes
    const cv::Scalar white(255, 255, 255);
    for (int j = 0; j < n; ++j)
    {
        double b = 0.55 + 0.45 * double(j) / n;
        cv::Scalar mid = col * b;
        cv::Scalar core = (col * 0.35 + white * 0.65) * b;
        cv::line(sub, pts[j] - off, pts[j + 1] - off, mid, 4, cv::LINE_AA);
        cv::line(sub, pts[j] - off, pts[j + 1] - off, core, 2, cv::LINE_AA);

        if (j > 0 && j < n - 1 && rnd(j * 13 + seed, tick + 7) > 0.5f)
        {
            int side = rnd(j, tick + 3) > 0 ? 1 : -1;
            cv::Point mid1 = pts[j] + cv::Point(dir * 10, side * 14);
            cv::Point end1 = mid1 + cv::Point(dir * 8, side * 12);
            cv::line(sub, pts[j] - off, mid1 - off, mid, 2, cv::LINE_AA);
            cv::line(sub, mid1 - off, end1 - off, mid, 1, cv::LINE_AA);
        }
    }
}

// Desenha um sprite com o canto superior esquerdo em 'tl'.
static void drawSpriteTL(cv::Mat &view, const cv::Mat &spr, cv::Point tl)
{
    drawSprite(view, spr, tl + cv::Point(spr.cols / 2, spr.rows / 2));
}

// Cor do brilho (BGR) conforme o feitico lancado.
static cv::Scalar castGlowColor(SpellType t)
{
    switch (t)
    {
    case SpellType::AtaqueFraco:
        return cv::Scalar(30, 120, 255); // laranja
    case SpellType::AtaqueMedio:
        return cv::Scalar(255, 220, 120); // azul claro
    case SpellType::AtaqueForte:
        return cv::Scalar(50, 90, 220); // vermelho
    case SpellType::Escudo:
        return cv::Scalar(255, 230, 100); // ciano
    case SpellType::AtaqueEspecial:
        return cv::Scalar(255, 80, 220); // magenta
    default:
        return cv::Scalar(255, 255, 255);
    }
}

// Brilho aditivo ao redor do mago + anel que se expande. g01: 1 = recem lancado, 0 = apagado.
static void drawCastGlow(cv::Mat &view, cv::Point c, int radius,
                         const cv::Scalar &col, float g01)
{
    g01 = std::clamp(g01, 0.f, 1.f);
    if (g01 <= 0.f || radius <= 0)
        return;

    const int pad = radius * 2;
    cv::Rect roi(c.x - pad, c.y - pad, 2 * pad, 2 * pad);
    roi &= cv::Rect(0, 0, view.cols, view.rows);
    if (roi.empty())
        return;

    cv::Mat glow = cv::Mat::zeros(roi.size(), view.type());
    cv::circle(glow, c - roi.tl(), cvRound(radius * 0.6), col * (0.9 * g01),
               cv::FILLED, cv::LINE_AA);
    cv::GaussianBlur(glow, glow, {0, 0}, radius * 0.3);
    cv::Mat sub = view(roi);
    cv::add(sub, glow, sub);

    // anel que cresce enquanto o brilho some
    const int ringR = cvRound(radius * (0.35 + 0.65 * (1.f - g01)));
    cv::circle(view, c, ringR, col * g01, 2, cv::LINE_AA);
}

// Cor media (BGR) do sprite, ponderada pelo alfa e normalizada para ficar brilhante.
// Amostra uma grade de ate ~24x24 pixels: custo baixo, sem precisar de cache.
static cv::Scalar spriteGlowColor(const cv::Mat &spr)
{
    if (spr.empty() || spr.depth() != CV_8U || spr.channels() < 3)
        return cv::Scalar(255, 255, 255);

    const int stepY = std::max(1, spr.rows / 24);
    const int stepX = std::max(1, spr.cols / 24);
    const int cn = spr.channels();
    double sb = 0, sg = 0, sr = 0, sw = 0;
    for (int y = 0; y < spr.rows; y += stepY)
    {
        const uchar *row = spr.ptr<uchar>(y);
        for (int x = 0; x < spr.cols; x += stepX)
        {
            const uchar *px = row + x * cn;
            const double a = (cn == 4) ? px[3] / 255.0 : 1.0;
            if (a < 0.1)
                continue;
            sb += px[0] * a;
            sg += px[1] * a;
            sr += px[2] * a;
            sw += a;
        }
    }
    if (sw <= 0.0)
        return cv::Scalar(255, 255, 255);

    cv::Scalar c(sb / sw, sg / sw, sr / sw);
    const double mx = std::max({c[0], c[1], c[2], 1.0});
    const double k = 255.0 / mx; // brilho maximo mantendo o matiz
    return cv::Scalar(c[0] * k, c[1] * k, c[2] * k);
}

// Brilho pulsante aditivo ao redor de um feitico em voo.
static void drawSpellGlow(cv::Mat &view, cv::Point c, int radius,
                          const cv::Scalar &col, float time)
{
    if (radius <= 0)
        return;
    const double pulse = 0.65 + 0.35 * std::sin(time * 14.0);

    const int pad = radius * 2;
    cv::Rect roi(c.x - pad, c.y - pad, 2 * pad, 2 * pad);
    roi &= cv::Rect(0, 0, view.cols, view.rows);
    if (roi.empty())
        return;

    cv::Mat glow = cv::Mat::zeros(roi.size(), view.type());
    cv::circle(glow, c - roi.tl(), cvRound(radius * 0.5), col * (0.8 * pulse),
               cv::FILLED, cv::LINE_AA);
    cv::GaussianBlur(glow, glow, {0, 0}, radius * 0.4);
    cv::Mat sub = view(roi);
    cv::add(sub, glow, sub);
}

void drawGame(cv::Mat &view, const Match &m, const Assets &as, bool glowEffects)
{
    if (!as.background.empty())
    {
        if (as.background.size() == view.size())
            as.background.copyTo(view);
        else
        {
            cv::Mat background;
            cv::resize(as.background, background, view.size());
            background.copyTo(view);
        }
    }
    else
        view.setTo(cv::Scalar(30, 20, 20));

    const int W = view.cols, H = view.rows;
    const int xs[2] = {160, W - 160};

    // magos e escudos
    for (int p = 0; p < 2; ++p)
    {
        // brilho atras do mago ao lancar feitico
        if (glowEffects)
            drawCastGlow(view, {xs[p], H / 2}, cvRound(H * 0.45f),
                         castGlowColor(m.castType(p)), m.castGlow01(p));

        const auto &fr = as.wizard[p];
        const float hit = m.hitFlash01(p);

        // tremor: deslocamento pseudo-aleatorio que diminui junto com o efeito
        cv::Point wc(xs[p], H / 2);
        if (hit > 0.f)
        {
            const int tick = int(hit * kHitFlashTime * 60.f); // muda ~60x/s enquanto decai
            wc.x += int(rnd(tick, p * 2 + 1) * 6.f * hit);
            wc.y += int(rnd(tick, p * 2 + 2) * 3.f * hit);
        }

        if (!fr.empty())
        {
            int f = int(m.time() * 8) % int(fr.size()); // 8 quadros por segundo
            if (hit > 0.f)
                drawFit(view, tintRed(fr[f], hit), wc);
            else
                drawFit(view, fr[f], wc);
        }
        else
            cv::circle(view, wc, 30,
                       p == 0 ? cv::Scalar(255, 200, 80) : cv::Scalar(80, 200, 255), cv::FILLED);
        if (m.hasShield(p))
        {
            if (!as.shield[p].empty())
                drawFit(view, as.shield[p], {xs[p], H / 2});
            else
                cv::circle(view, {xs[p], H / 2}, 50, {255, 255, 255}, 3);
        }
    }

    for (const Projectile &pr : m.projectiles())
    {
        int x = xs[pr.owner] + int((xs[1 - pr.owner] - xs[pr.owner]) * pr.t);

        const auto &anim = pr.owner == 1 ? as.boltAnimFlipped[1][int(pr.type)]
                                         : as.boltAnim[0][int(pr.type)];
        const cv::Mat *spr = nullptr;
        if (!anim.empty())
        {
            int f = int(m.time() * 24) % int(anim.size()); // 24 quadros por segundo
            spr = &anim[f];
        }
        else
        {
            spr = pr.owner == 1 ? &as.boltFlipped[1][int(pr.type)]
                                : &as.bolt[0][int(pr.type)];
        }

        if (!spr->empty())
        {
            // brilho com a cor media do sprite (atras do feitico)
            if (glowEffects)
            {
                const double fit = spr->rows > H - 4 ? double(H - 4) / spr->rows : 1.0;
                const int glowR = std::min(cvRound(std::max(spr->cols, spr->rows) * fit * 0.7),
                                           cvRound(H * 0.5));
                drawSpellGlow(view, {x, H / 2}, glowR, spriteGlowColor(*spr), m.time());
            }

            if (pr.type == SpellType::AtaqueMedio) // Raio: corta a arena inteira
            {
                const int dir = (pr.owner == 0) ? 1 : -1;
                const cv::Point tail(xs[pr.owner] + dir * 40, H / 2);
                const cv::Point head(x - dir * (spr->cols / 3), H / 2);
                drawLightningTrail(view, tail, head, as.trailColor[pr.owner],
                                   m.time(), pr.owner, glowEffects);
            }
            drawFit(view, *spr, {x, H / 2});
        }
        else
            cv::circle(view, {x, H / 2}, 10 + int(pr.damage / 3), {80, 80, 255}, cv::FILLED);
    }

    if (m.state() != MatchState::Running)
        cv::putText(view, m.state() == MatchState::Player0Won ? "JOGADOR 1 VENCEU" : "JOGADOR 2 VENCEU",
                    {W / 2 - 200, H / 2 - 55}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 3);
}

// ---------------------------------------------------------------------------
// Barras de vida/mana usadas pelo HUD superior (chamadas pelo main.cpp)
// ---------------------------------------------------------------------------

static const BarSkin &skinFor(const Assets &as, BarKind kind)
{
    return kind == BarKind::Health ? as.healthBar : as.manaBar;
}

cv::Size statBarSize(const Assets &as, const cv::Size &maxSize)
{
    const BarSkin &sk = as.healthBar.valid() ? as.healthBar : as.manaBar;
    if (!sk.valid())
        return {std::max(1, maxSize.width), std::max(1, std::min(maxSize.height, 22))};

    const cv::Mat &f = sk.frame[0];
    const double s = std::min({double(maxSize.width) / f.cols,
                               double(maxSize.height) / f.rows, 1.0});
    return {std::max(1, cvRound(f.cols * s)), std::max(1, cvRound(f.rows * s))};
}

void drawStatBar(cv::Mat &dst, const Assets &as, BarKind kind, int side,
                 float v01, const cv::Rect &rect, const std::string &label)
{
    v01 = std::clamp(v01, 0.f, 1.f);
    side = std::clamp(side, 0, 1);
    const BarSkin &sk = skinFor(as, kind);
    cv::Rect textArea = rect;

    if (!sk.valid())
    {
        // Fallback: retangulo simples
        const cv::Scalar col = kind == BarKind::Health ? cv::Scalar(60, 60, 230)
                                                       : cv::Scalar(230, 160, 50);
        cv::rectangle(dst, rect, {20, 20, 20}, cv::FILLED);
        const int w = cvRound((rect.width - 4) * v01);
        const int x = side == 0 ? rect.x + 2 : rect.x + rect.width - 2 - w;
        if (w > 0)
            cv::rectangle(dst, cv::Rect(x, rect.y + 2, w, rect.height - 4), col, cv::FILLED);
        cv::rectangle(dst, rect, {255, 255, 255}, 1);
    }
    else
    {
        // Moldura redimensionada para o rect
        const cv::Mat &src = sk.frame[side];
        const double sx = double(rect.width) / src.cols;
        const double sy = double(rect.height) / src.rows;
        cv::Mat frame;
        cv::resize(src, frame, rect.size(), 0, 0, cv::INTER_AREA);
        drawSpriteTL(dst, frame, rect.tl());

        // Slot interno escalado
        const cv::Rect &sl = sk.slot[side];
        const cv::Rect slot(cvRound(sl.x * sx), cvRound(sl.y * sy),
                            std::max(1, cvRound(sl.width * sx)),
                            std::max(1, cvRound(sl.height * sy)));
        textArea = cv::Rect(rect.x + slot.x, rect.y + slot.y, slot.width, slot.height);

        const int w = cvRound(slot.width * v01);
        if (w > 0)
        {
            cv::Mat fill;
            cv::resize(sk.fill[side], fill, slot.size(), 0, 0, cv::INTER_AREA);
            const int srcX = side == 0 ? 0 : slot.width - w;
            const int dstX = side == 0 ? slot.x : slot.x + slot.width - w;
            drawSpriteTL(dst, fill(cv::Rect(srcX, 0, w, slot.height)),
                         rect.tl() + cv::Point(dstX, slot.y));
        }
    }

    if (!label.empty())
    {
        const double scale = std::clamp(textArea.height / 28.0, 0.3, 0.6);
        int base = 0;
        const cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, scale, 1, &base);
        const cv::Point org(textArea.x + (textArea.width - ts.width) / 2,
                            textArea.y + (textArea.height + ts.height) / 2);
        cv::putText(dst, label, org + cv::Point(1, 1), cv::FONT_HERSHEY_SIMPLEX, scale,
                    {0, 0, 0}, 2, cv::LINE_AA);
        cv::putText(dst, label, org, cv::FONT_HERSHEY_SIMPLEX, scale,
                    {255, 255, 255}, 1, cv::LINE_AA);
    }
}