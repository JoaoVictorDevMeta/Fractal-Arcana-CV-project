#pragma once
#include <opencv2/videoio.hpp>
#include <opencv2/core.hpp>

class Camera {
public:
    bool open(int index, int w = 1280, int h = 720, int fps = 60);
    bool read(cv::Mat& out);
    void close();
    bool isOpen() { return cap_.isOpened(); }

private:
    cv::VideoCapture cap_;
};