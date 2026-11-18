#include "vision/air_pen.h"
#include <opencv2/imgproc.hpp>

void AirPen::reset(cv::Size size) {
    canvas = cv::Mat::zeros(size, CV_8UC3);
    hasLast_ = hasSmooth_ = false;
    missed_ = 0;
    resetStroke();
}

void AirPen::resetStroke() {
    currentStroke_.clear();
    strokePending_ = false;
    missed_ = 0;
    idleFrames_ = 0;
}

void AirPen::update(Detection& d) {
    if (canvas.empty()) return;

    if (!d.found) {
        missed_++;
        if (missed_ > maxMissed && hasLast_) {
            hasLast_ = hasSmooth_ = false;
            if (assistant_.totalLength(currentStroke_) > 100.0f) {
                strokePending_ = true;
            } else {
                resetStroke();
            }
        }
        return;
    }

    if (missed_ > maxMissed) {
        strokePending_ = false;
    }
    missed_ = 0;

    if (!penDown) {
        hasLast_ = hasSmooth_ = false;
        return;
    }

    cv::Point2f p = d.center;
    if (hasSmooth_) p = alpha * d.center + (1.f - alpha) * smooth_;
    smooth_ = p;
    hasSmooth_ = true;

    if (hasLast_) {
        // Só registra ponto no vetor se houve deslocamento real (evita acúmulo de pontos parados)
        if (currentStroke_.empty() || cv::norm(p - currentStroke_.back()) >= minStepDist) {
            cv::line(canvas,
                     cv::Point(cvRound(last_.x), cvRound(last_.y)),
                     cv::Point(cvRound(p.x), cvRound(p.y)),
                     color, thickness, cv::LINE_AA);
            currentStroke_.push_back(p);
        }

        if (idleFrames_ == 0) {
            idleAnchor_ = p;
        }

        float distFromAnchor = cv::norm(p - idleAnchor_);
        if (distFromAnchor < IDLE_RADIUS) {
            idleFrames_++;
        } else {
            idleAnchor_ = p;
            idleFrames_ = 0;
        }

        // Como pontos parados não são adicionados ao vetor, não precisamos apagar o fim do traço
        if (idleFrames_ >= MAX_IDLE_FRAMES) {
            if (assistant_.totalLength(currentStroke_) > 100.0f) {
                strokePending_ = true;
            } else {
                resetStroke();
            }
            idleFrames_ = 0;
        }
    } else {
        currentStroke_.push_back(p);
    }

    last_ = p;
    hasLast_ = true;
}

void AirPen::renderAssistant(cv::Mat& frame) {
    assistant_.renderOverlay(frame, currentStroke_);
}

std::vector<cv::Point2f> AirPen::getStroke() {
    return assistant_.smoothStroke(currentStroke_, 2);
}

bool AirPen::hasFinishedStroke() {
    return strokePending_;
}