#include "core/hsv_range.h"
#include <opencv2/core.hpp>

bool saveRange(const std::string& path, const HsvRange& r) {
    cv::FileStorage fs(path, cv::FileStorage::WRITE);
    if (!fs.isOpened()) return false;
    fs << "wand" << "{"
       << "h_min" << r.hLo << "h_max" << r.hHi
       << "s_min" << r.sLo << "s_max" << r.sHi
       << "v_min" << r.vLo << "v_max" << r.vHi << "}";
    return true;
}

bool loadRange(const std::string& path, HsvRange& r) {
    cv::FileStorage fs(path, cv::FileStorage::READ);
    if (!fs.isOpened()) return false;
    cv::FileNode n = fs["wand"];
    if (n.empty()) return false;
    n["h_min"] >> r.hLo; n["h_max"] >> r.hHi;
    n["s_min"] >> r.sLo; n["s_max"] >> r.sHi;
    n["v_min"] >> r.vLo; n["v_max"] >> r.vHi;
    return true;
}