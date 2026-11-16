#include "vision/camera.h"
#include <opencv2/imgproc.hpp>

bool Camera::open(int index, int w, int h, int fps) {
    cap_.open(index);
    if (!cap_.isOpened()) return false;
    cap_.set(cv::CAP_PROP_FRAME_WIDTH, w);
    cap_.set(cv::CAP_PROP_FRAME_HEIGHT, h);
    cap_.set(cv::CAP_PROP_FPS, fps);
    cap_.set(cv::CAP_PROP_BUFFERSIZE, 1);
    return true;
}

bool Camera::read(cv::Mat& out) {
    if (!cap_.read(out) || out.empty()) return false;
    cv::flip(out, out, 1);
    return true;
}

void Camera::close() { cap_.release(); }