#pragma once
#include <opencv2/core.hpp>
#include <string>
#include <vector>

constexpr int kWizardFrames = 8;
// Carrega PNG mantendo alfa (BGRA). Se height > 0, redimensiona mantendo proporção.
// Retorna Mat vazia se falhar.
cv::Mat loadSprite(const std::string &path, int height = 0);

// Desenha 'spr' (BGRA) centrado em 'center', com blend de alfa e clipping nas bordas.
void drawSprite(cv::Mat &dst, const cv::Mat &spr, cv::Point center);
std::vector<cv::Mat> loadStrip(const std::string &path, int frameW, int height);

struct Assets
{
    std::vector<cv::Mat> wizard[2]; // [jogador][frame]
    cv::Mat shield;
    cv::Mat bolt[6];                  // estáticos (fallback)
    std::vector<cv::Mat> boltAnim[6]; // animados: [SpellType][frame]
    cv::Mat background;               // opcional, BGR

    bool load(const std::string &dir);
};