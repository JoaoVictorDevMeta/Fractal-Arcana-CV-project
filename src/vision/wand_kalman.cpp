#include "vision/wand_kalman.h"
#include <algorithm>

WandKalman::WandKalman() : kf_(4, 2, 0, CV_32F) {}

void WandKalman::start_(const cv::Point2f &p)
{
    // estado [x, y, vx, vy]; medida [x, y]
    kf_.transitionMatrix = cv::Mat(cv::Matx44f(
        1, 0, 1, 0,
        0, 1, 0, 1,
        0, 0, 1, 0,
        0, 0, 0, 1)); // os "1" de dt sao reescritos a cada frame
    kf_.measurementMatrix = cv::Mat(cv::Matx<float, 2, 4>(
        1, 0, 0, 0,
        0, 1, 0, 0));
    kf_.processNoiseCov = cv::Mat(cv::Matx44f(
        processNoisePos, 0, 0, 0,
        0, processNoisePos, 0, 0,
        0, 0, processNoiseVel, 0,
        0, 0, 0, processNoiseVel));
    cv::setIdentity(kf_.measurementNoiseCov, cv::Scalar::all(measureNoise));
    cv::setIdentity(kf_.errorCovPost, cv::Scalar::all(100.0));
    kf_.statePost = cv::Mat(cv::Vec4f(p.x, p.y, 0.f, 0.f));
    kf_.statePre = kf_.statePost.clone();
    ready_ = true;
    lost_ = 0;
}

void WandKalman::reset()
{
    ready_ = false;
    lost_ = 0;
}

Tracked WandKalman::update(const Detection &d, double t)
{
    Tracked out;

    if (!ready_)
    {
        if (!d.found)
            return out;
        start_(d.center);
        lastT_ = t;
        out.valid = out.detected = true;
        out.pos = d.center;
        return out;
    }

    const double dt = std::clamp(t - lastT_, 1.0 / 120.0, 0.1);
    lastT_ = t;
    kf_.transitionMatrix.at<float>(0, 2) = static_cast<float>(dt);
    kf_.transitionMatrix.at<float>(1, 3) = static_cast<float>(dt);

    cv::Mat est = kf_.predict();
    if (d.found)
    {
        const cv::Mat z = cv::Mat(cv::Vec2f(d.center.x, d.center.y));
        est = kf_.correct(z);
        lost_ = 0;
        out.detected = true;
    }
    else if (++lost_ > maxLost)
    {
        reset();
        return out;
    }

    out.valid = true;
    out.pos = {est.at<float>(0), est.at<float>(1)};
    out.vel = {est.at<float>(2), est.at<float>(3)};
    return out;
}