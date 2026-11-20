#pragma once
#include "vision/wand_detector.h"
#include <opencv2/core.hpp>
#include <opencv2/video/tracking.hpp>

// Resultado do filtro para um frame.
struct Tracked {
    bool valid = false;     // existe posicao utilizavel (medida OU predita)
    bool detected = false;  // veio de uma deteccao real neste frame
    cv::Point2f pos{0.f, 0.f};
    cv::Point2f vel{0.f, 0.f};  // px/s
};

// Kalman (velocidade constante) que suaviza a saida do WandDetector
// e cobre pequenas perdas de deteccao com a posicao prevista.
class WandKalman {
public:
    int   maxLost       = 10;    // frames seguidos sem deteccao antes de desistir
    float processNoisePos = 1.f;   // Q posicao  (maior = menos suavizacao)
    float processNoiseVel = 50.f;  // Q velocidade
    float measureNoise  = 4.f;     // R (maior = confia menos na camera, suaviza mais)

    WandKalman();

    // d: deteccao em coordenadas do frame inteiro; t: tempo em segundos.
    Tracked update(const Detection& d, double t);
    void reset();

private:
    void start_(const cv::Point2f& p);

    cv::KalmanFilter kf_;
    bool   ready_ = false;
    int    lost_  = 0;
    double lastT_ = 0.0;
};