#include "vision/split_screen.h"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <cstdio>

SplitScreen::SplitScreen() {
    pen[0].color = cv::Scalar(255, 128, 0);  // P1: azul
    pen[1].color = cv::Scalar(0, 255, 0);    // P2: verde
    // O Kalman ja suaviza; sem isso a tinta teria suavizacao dupla (atraso).
    for (auto& p : pen) p.alpha = 1.0f;
}

void SplitScreen::configure_(const cv::Size& size) {
    size_ = size;
    const int half = size.width / 2;
    zone_[0] = cv::Rect(0, 0, half, size.height);
    zone_[1] = cv::Rect(half, 0, size.width - half, size.height);
    for (int i = 0; i < 2; ++i) {
        kalman[i].reset();
        pen[i].reset(size);  // canvas no tamanho do frame inteiro
    }
}

void SplitScreen::update(const cv::Mat& frame, double t) {
    if (frame.size() != size_) configure_(frame.size());

    for (int i = 0; i < 2; ++i) {
        // O detector existente roda so na metade do jogador (view, sem copia).
        Detection d = detector[i].detect(frame(zone_[i]), &mask_[i]);
        if (d.found) {  // volta para coordenadas do frame inteiro
            d.center += cv::Point2f(zone_[i].tl());
            d.bbox   += zone_[i].tl();
        }

        Tracked k = kalman[i].update(d, t);
        if (k.valid) {  // mantem o ponto dentro da metade do jogador
            k.pos.x = std::clamp(k.pos.x, float(zone_[i].x), float(zone_[i].br().x - 1));
            k.pos.y = std::clamp(k.pos.y, float(zone_[i].y), float(zone_[i].br().y - 1));
        }
        view_[i] = {d, k};

        // A tinta recebe a posicao suavizada (inclui frames previstos).
        Detection fed = d;
        fed.found  = k.valid;
        fed.center = k.pos;
        pen[i].update(fed);
    }
}

void SplitScreen::draw(cv::Mat& f) const {
    cv::Mat gray, ink;
    for (int i = 0; i < 2; ++i) {
        cv::cvtColor(pen[i].canvas, gray, cv::COLOR_BGR2GRAY);
        cv::threshold(gray, ink, 0, 255, cv::THRESH_BINARY);
        pen[i].canvas.copyTo(f, ink);
    }

    const int mid = zone_[1].x;
    cv::line(f, {mid, 0}, {mid, f.rows}, {255, 255, 255}, 1, cv::LINE_AA);

    for (int i = 0; i < 2; ++i) {
        const auto& v = view_[i];
        if (v.det.found) cv::rectangle(f, v.det.bbox, pen[i].color, 2);
        if (v.trk.valid) {
            const cv::Point p(cvRound(v.trk.pos.x), cvRound(v.trk.pos.y));
            if (v.trk.detected) cv::circle(f, p, 7, pen[i].color, cv::FILLED, cv::LINE_AA);
            else                cv::circle(f, p, 9, {0, 255, 255}, 2, cv::LINE_AA);  // so previsao
        }
        const char* st = !v.trk.valid ? "PERDIDA" : (v.trk.detected ? "DETECTADA" : "PREVISTA");
        char line[64];
        std::snprintf(line, sizeof(line), "P%d: %s", i + 1, st);
        cv::putText(f, line, {zone_[i].x + 10, 28}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                    pen[i].color, 2, cv::LINE_AA);
    }
}

void SplitScreen::clearPens() {
    for (auto& p : pen) p.reset(size_);
}

void SplitScreen::setPenDown(bool down) {
    for (auto& p : pen) p.penDown = down;
}

cv::Mat SplitScreen::combinedMask() const {
    cv::Mat out = cv::Mat::zeros(size_, CV_8UC1);
    for (int i = 0; i < 2; ++i)
        if (!mask_[i].empty() && mask_[i].size() == zone_[i].size())
            mask_[i].copyTo(out(zone_[i]));
    return out;
}