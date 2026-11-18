#include "render/hud.h"
#include "game/match.h"
#include <opencv2/imgproc.hpp>

static void drawBar(cv::Mat &v, cv::Rect r, float frac, cv::Scalar color)
{
    cv::rectangle(v, r, {60, 60, 60}, cv::FILLED);
    cv::Rect fill = r;
    fill.width = int(r.width * std::clamp(frac, 0.f, 1.f));
    cv::rectangle(v, fill, color, cv::FILLED);
    cv::rectangle(v, r, {255, 255, 255}, 1);
}

void drawGame(cv::Mat &view, const Match &m)
{
    view.setTo(cv::Scalar(30, 20, 20)); // fundo
    cv::line(view, {view.cols / 2, 0}, {view.cols / 2, view.rows}, {80, 80, 80}, 1);

    // barras: vida (vermelha) e mana (azul) do jogador 1, a esquerda
    drawBar(view, {20, 20, 300, 20}, m.vida01(), {60, 60, 230});
    drawBar(view, {20, 48, 300, 14}, m.mana01(), {230, 140, 40});
}