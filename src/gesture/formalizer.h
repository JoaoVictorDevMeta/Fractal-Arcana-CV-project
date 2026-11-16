#pragma once
#include "gesture/shapes.h"
#include <vector>

std::vector<cv::Point2f> resample(const std::vector<cv::Point2f>& pts, int n = 64);

std::vector<cv::Point2f> normalize(const std::vector<cv::Point2f>& pts);