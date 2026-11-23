#include "render/hud.h"
#include "render/sprites.h"
#include "game/match.h"
#include <opencv2/imgproc.hpp>

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

// Rastro de raio atras do projetil: head = ponta, dir = +1 (P1) ou -1 (P2).
static void drawLightningTrail(cv::Mat &view, cv::Point tail, cv::Point head,
                               const cv::Scalar &col, float time, int seed)
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
        if (j == 0) y = tail.y;
        if (j == n) y = head.y;
        pts.push_back({x, y});
    }

    cv::Rect roi(std::min(tail.x, head.x) - 25, tail.y - 60, len + 50, 120);
    roi &= cv::Rect(0, 0, view.cols, view.rows);
    if (roi.empty())
        return;
    const cv::Point off = roi.tl();

    // brilho (camada aditiva desfocada)
    cv::Mat glow = cv::Mat::zeros(roi.size(), view.type());
    for (int j = 0; j < n; ++j)
    {
        double b = 0.55 + 0.45 * double(j) / n; // mais forte perto do projetil
        cv::line(glow, pts[j] - off, pts[j + 1] - off, col * b, 9, cv::LINE_AA);
    }
    cv::GaussianBlur(glow, glow, {13, 13}, 0);
    cv::Mat sub = view(roi);
    cv::add(sub, glow, sub);

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

void drawGame(cv::Mat &view, const Match &m, const Assets &as)
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
        const auto &fr = as.wizard[p];
        if (!fr.empty())
        {
            int f = int(m.time() * 8) % int(fr.size()); // 8 quadros por segundo
            drawFit(view, fr[f], {xs[p], H / 2});
        }
        else
            cv::circle(view, {xs[p], H / 2}, 30,
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
            if (pr.type == SpellType::AtaqueMedio) // Raio: corta a arena inteira
            {
                const int dir = (pr.owner == 0) ? 1 : -1;
                const cv::Point tail(xs[pr.owner] + dir * 40, H / 2);
                const cv::Point head(x - dir * (spr->cols / 3), H / 2);
                drawLightningTrail(view, tail, head, as.trailColor[pr.owner],
                                   m.time(), pr.owner);
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