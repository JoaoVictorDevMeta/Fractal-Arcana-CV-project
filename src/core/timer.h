#pragma once
#include <opencv2/core.hpp>
#include <algorithm>

// Relogio do loop do jogo: fonte unica de dt e do tempo absoluto.
// O dt e limitado a maxDt_ para que uma travada de um frame (queda da
// camera, arrastar a janela, waitKey bloqueando) nao provoque um salto
// na simulacao (mana/cooldown pulando, projetil atravessando o alvo).
class FrameTimer
{
public:
    explicit FrameTimer(double maxDt = 0.1) : maxDt_(maxDt) { reset(); }

    void reset() { tPrev_ = cv::getTickCount(); now_ = 0.0; dt_ = 0.0; }

    // Avanca um frame e retorna o dt em segundos (ja limitado a maxDt_).
    double tick()
    {
        const int64 t = cv::getTickCount();
        dt_ = std::clamp((t - tPrev_) / cv::getTickFrequency(), 0.0, maxDt_);
        tPrev_ = t;
        now_ += dt_;
        return dt_;
    }

    double dt() const { return dt_; }
    double now() const { return now_; }

private:
    int64 tPrev_ = 0;
    double dt_ = 0.0;
    double now_ = 0.0;
    double maxDt_ = 0.1;
};
