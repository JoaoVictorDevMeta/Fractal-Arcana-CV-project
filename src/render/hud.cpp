#include "render/hud.h"
#include "render/sprites.h"
#include "game/match.h"
#include <algorithm>
#include <opencv2/imgproc.hpp>

static void drawBar(cv::Mat &v, cv::Rect r, float frac, cv::Scalar color)
{
    cv::rectangle(v, r, {60, 60, 60}, cv::FILLED);
    cv::Rect fill = r;
    fill.width = int(r.width * std::clamp(frac, 0.f, 1.f));
    cv::rectangle(v, fill, color, cv::FILLED);
    cv::rectangle(v, r, {255, 255, 255}, 1);
}

void drawGame(cv::Mat &view, const Match &m, const Assets &as)
{
    if (!as.background.empty())
        as.background.copyTo(view);
    else
        view.setTo(cv::Scalar(30, 20, 20));

    const int W = view.cols, H = view.rows;
    const int xs[2] = {160, W - 160};

    // barras: jogador 0 a esquerda, jogador 1 a direita
    drawBar(view, {20, 20, 300, 20}, m.health01(0), {60, 60, 230});
    drawBar(view, {20, 48, 300, 14}, m.mana01(0), {230, 140, 40});
    drawBar(view, {W - 320, 20, 300, 20}, m.health01(1), {60, 60, 230});
    drawBar(view, {W - 320, 48, 300, 14}, m.mana01(1), {230, 140, 40});

    // magos e escudos
    for (int p = 0; p < 2; ++p)
    {
        const auto &fr = as.wizard[p];
        if (!fr.empty())
        {
            int f = int(m.time() * 8) % int(fr.size()); // 8 quadros por segundo
            drawSprite(view, fr[f], {xs[p], H / 2});
        }
        else
            cv::circle(view, {xs[p], H / 2}, 30,
                       p == 0 ? cv::Scalar(255, 200, 80) : cv::Scalar(80, 200, 255), cv::FILLED);
        if (m.hasShield(p))
        {
            if (!as.shield.empty())
                drawSprite(view, as.shield, {xs[p], H / 2});
            else
                cv::circle(view, {xs[p], H / 2}, 50, {255, 255, 255}, 3);
        }
    }

    for (const Projectile &pr : m.projectiles())
    {
        int x = xs[pr.owner] + int((xs[1 - pr.owner] - xs[pr.owner]) * pr.t);

        const auto &anim = as.boltAnim[int(pr.type)];
        const cv::Mat *spr = nullptr;
        if (!anim.empty())
        {
            int f = int(m.time() * 24) % int(anim.size()); // 24 quadros por segundo
            spr = &anim[f];
        }
        else
            spr = &as.bolt[int(pr.type)];

        if (!spr->empty())
        {
            if (pr.owner == 1)
            {
                cv::Mat f;
                cv::flip(*spr, f, 1);
                drawSprite(view, f, {x, H / 2});
            }
            else
                drawSprite(view, *spr, {x, H / 2});
        }
        else
            cv::circle(view, {x, H / 2}, 10 + int(pr.damage / 3), {80, 80, 255}, cv::FILLED);
    }
    if (m.state() != MatchState::Running)
        cv::putText(view, m.state() == MatchState::Player0Won ? "JOGADOR 1 VENCEU" : "JOGADOR 2 VENCEU",
                    {W / 2 - 200, H / 2 - 100}, cv::FONT_HERSHEY_SIMPLEX, 1.2, {255, 255, 255}, 3);
}