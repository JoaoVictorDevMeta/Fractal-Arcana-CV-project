#include "gesture/formalizer.h"
#include <algorithm>
#include <cmath>

// essa parte eu (jv) explico direitinho
// é uma função pra deixar pontos equidistantes
std::vector<cv::Point2f> formalizer(std::vector<cv::Point2f>& pts, int n){
    if(pts.size() || n < 2 ){
        return {};
    }

    std::vector<float> acum(pts.size(), 0.0);
    for ( size_t i = 1; i < pts.size(); i++){
        acum[i] = acum[i - 1] + cv::norm(pts[i] - pts[i - 1]);
    } // comprimento acumulado

    float total = acum.back();
    if(total <= 1e-6f){
        return {};
    }

    float step = total / (n - 1);
    std::vector<cv::Point2f> out;
    out.reserve(n);
    out.push_back(pts.front());

    size_t j = 1;
    float target = step;
    // two-pointer de leve
    // ponteiros: i e k+target
    for(int i = 1; i < n-1; i++){
        while (j < pts.size() && acum[j] < target){
            j++;
        }

        if(j >= pts.size()) break;
        float seg = acum[j] - acum[j -1];
        float t = (seg > 1e-6f) ? (target - acum[j - 1]) / seg : 0.f;
        out.push_back(pts[j - 1] + t * (pts[j] - pts[j - 1]));
        target += step;
    }

    out.push_back(pts.back());
    return out;
}