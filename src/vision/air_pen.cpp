#include "vision/air_pen.h"
#include <opencv2/imgproc.hpp>

void AirPen::reset(const cv::Size& size) {
    canvas = cv::Mat::zeros(size, CV_8UC3);
    hasLast_ = hasSmooth_ = false;
    missed_ = 0;
    resetStroke(); // Garante que o traço seja limpo ao dar reset na tela
}

void AirPen::resetStroke() {
    currentStroke_.clear();
    strokePending_ = false;
    missed_ = 0;
    idleFrames_ = 0;
}

void AirPen::update(const Detection& d) {
    if (canvas.empty()) return;

    if (!d.found) {
        missed_++;
        if (missed_ > maxMissed && hasLast_) {
            hasLast_ = hasSmooth_ = false;
            strokePending_ = true; // Avisa que o traço terminou e aguarda classificação
        }
        return;
    }

    if (missed_ > maxMissed) {
        strokePending_ = false;
    }
    missed_ = 0;

    if (!penDown) { hasLast_ = hasSmooth_ = false; return; }

    cv::Point2f p = d.center;
    if (hasSmooth_) p = alpha * d.center + (1.f - alpha) * smooth_;
    smooth_ = p; hasSmooth_ = true;

    if (hasLast_) {
        // Desenha na tela
        cv::line(canvas,
                 cv::Point(cvRound(last_.x), cvRound(last_.y)),
                 cv::Point(cvRound(p.x),    cvRound(p.y)),
                 color, thickness, cv::LINE_AA);
        currentStroke_.push_back(p);

        // 2. LÓGICA DE PAUSA (IDLE) - "Segurar parado para lançar"
        if (idleFrames_ == 0) {
            idleAnchor_ = p; // Marca onde ele começou a desacelerar/parar
        }

        float distFromAnchor = cv::norm(p - idleAnchor_);
        
        if (distFromAnchor < IDLE_RADIUS) {
            idleFrames_++; // Ele está quase parado
        } else {
            idleAnchor_ = p; // Ele voltou a se mover
            idleFrames_ = 0;
        }

        // Se ficou parado tempo suficiente:
        if (idleFrames_ >= MAX_IDLE_FRAMES) {
            // CORTE INTELIGENTE: Remove os pontos "parados" do final do traço
            // Assim a IA não vê o "borrão" que a varinha parada criou.
            if (currentStroke_.size() > (size_t)idleFrames_) {
                currentStroke_.resize(currentStroke_.size() - idleFrames_);
            }
            
            strokePending_ = true;
            idleFrames_ = 0; 
        }
    }
    last_ = p; hasLast_ = true;
}

std::vector<cv::Point2f> AirPen::getStroke() {
    return currentStroke_;
}

bool AirPen::hasFinishedStroke() {
    return strokePending_;
}