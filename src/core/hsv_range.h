#pragma once
#include <string>

struct HsvRange
{
    int hLo = 100, hHi = 130;
    int sLo = 120, sHi = 255;
    int vLo = 100, vHi = 255;
};

bool saveRange(const std::string &path, const HsvRange &r);
bool loadRange(const std::string &path, HsvRange &r);