#pragma once
#include "gesture/shapes.h"
#include <opencv2/core.hpp>
#include <string>
#include <vector>

// Pecas de interface reaproveitadas pelo menu (selecao, info) e pela tela de combate.
namespace ui
{
    // Cores dos jogadores (BGR): P1 laranja, P2 verde - as mesmas da selecao.
    cv::Scalar playerColor(int p);

    // Retangulo semitransparente (alpha 0..1) sobre a imagem.
    void fillTranslucent(cv::Mat &img, const cv::Rect &r, const cv::Scalar &col, double alpha);

    // Painel pixel art: fundo translucido, borda dupla e cantoneiras em 'accent'.
    void panel(cv::Mat &img, const cv::Rect &r, const cv::Scalar &accent, double fillAlpha = 0.82,
               const cv::Scalar &fill = cv::Scalar(26, 18, 34), int border = 3);

    // Faixa de titulo (texto centralizado, com contorno) no topo de um painel.
    void panelTitle(cv::Mat &img, const cv::Rect &panelRect, const std::string &title, const cv::Scalar &accent,
                    double scale = 0.6);

    // Quebra 'text' em linhas que cabem em maxW pixels.
    std::vector<std::string> wrapText(const std::string &text, int font, double scale, int thick, int maxW);

    // Escreve texto quebrado em linhas a partir de 'org' (linha de base); devolve o y da proxima linha.
    int putWrapped(cv::Mat &img, const std::string &text, cv::Point org, int maxW, int font, double scale,
                   const cv::Scalar &col, int thick = 1, int lineGap = 6);

    // Maior escala (<= maxScale) em que 'text' cabe em maxW.
    double fitScale(const std::string &text, int font, double maxScale, int thick, int maxW);

    // Icone da forma do gesto (linha, raio, circulo, triangulo, estrela) dentro de 'box'.
    void drawShapeIcon(cv::Mat &img, Shape s, const cv::Rect &box, const cv::Scalar &col, int thick = 2);

    // Estrela de 5 pontas cheia (indicador do especial).
    void drawStar(cv::Mat &img, cv::Point c, int r, const cv::Scalar &col, bool filled = true);

    // Aviso "PAUSADO" no centro da imagem.
    void pausedOverlay(cv::Mat &img);
} // namespace ui
