#pragma once
#include "vision/wand_detector.h"
#include "vision/wand_kalman.h"
#include "vision/air_pen.h"
#include <array>
#include <opencv2/core.hpp>

// Estado de um jogador em um frame (tudo em coordenadas do frame inteiro).
struct PlayerView {
    Detection det;  // deteccao bruta do WandDetector
    Tracked   trk;  // posicao suavizada pelo Kalman
};

// Divide o frame ao meio: esquerda = jogador 1, direita = jogador 2.
// Cada metade tem seu proprio WandDetector (uma cor), WandKalman e AirPen.
class SplitScreen {
public:
    std::array<WandDetector, 2> detector;
    std::array<WandKalman,   2> kalman;
    std::array<AirPen,       2> pen;

    SplitScreen();

    void update(const cv::Mat& frame, double t);  // frame ja espelhado
    void draw(cv::Mat& frame) const;              // tinta, divisoria, marcadores
    void clearPens();
    void setPenDown(bool down);
    cv::Mat combinedMask() const;                 // mascaras lado a lado (debug)

    const PlayerView& player(int i) const { return view_[i]; }
    const cv::Rect&   zone(int i)   const { return zone_[i]; }

private:
    void configure_(const cv::Size& size);

    cv::Size size_;
    std::array<cv::Rect,    2> zone_;
    std::array<cv::Mat,     2> mask_;
    std::array<PlayerView,  2> view_;
};