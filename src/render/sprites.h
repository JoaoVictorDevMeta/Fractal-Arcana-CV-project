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

// Carrega uma imagem de wizard como spritesheet (kWizardFrames frames) ou imagem unica.
// Preenche 'out' (lado 0 = normal, lado 1 = espelhado). Retorna true se carregou algo.
bool loadWizardFrames(const std::string &path, std::vector<cv::Mat> out[2]);

struct Assets
{
    std::vector<cv::Mat> wizard[2]; // [lado][frame]  lado-0 = esquerda, lado-1 = direita
    cv::Mat shield[2];
    cv::Mat bolt[2][6];                   // estaticos (fallback global) — lado P0
    cv::Mat boltFlipped[2][6];            // idem, pre-flipado para P1
    std::vector<cv::Mat> boltAnim[2][6];  // animados: [SpellType][frame] — lado P0
    std::vector<cv::Mat> boltAnimFlipped[2][6]; // idem, pre-flipado para P1
    cv::Mat background;               // opcional, BGR
    cv::Scalar trailColor[2] = {cv::Scalar(255, 220, 120), cv::Scalar(255, 220, 120)}; // cor do rastro do raio (BGR)

    // p1Id / p2Id: prefixo do personagem (ex: "derzu", "baltar") —
    // os arquivos esperados sao "sprites/players/<id>_player.png" e
    // "sprites/spells/<spell>_<id>.png" com fallback para os sprites padrao.
    bool load(const std::string &dir,
              const std::string &p1PlayerFile,
              const std::string &p2PlayerFile,
              const std::string &p1Id = "",
              const std::string &p2Id = "");
};