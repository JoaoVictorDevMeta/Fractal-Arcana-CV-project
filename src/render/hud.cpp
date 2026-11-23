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
            drawFit(view, *spr, {x, H / 2});
        }
        else
            cv::circle(view, {x, H / 2}, 10 + int(pr.damage / 3), {80, 80, 255}, cv::FILLED);
    }
    if (m.state() != MatchState::Running)
        cv::putText(view, m.state() == MatchState::Player0Won ? "JOGADOR 1 VENCEU" : "JOGADOR 2 VENCEU",
                    {W / 2 - 200, H / 2 - 55}, cv::FONT_HERSHEY_SIMPLEX, 1.0, {255, 255, 255}, 3);
}