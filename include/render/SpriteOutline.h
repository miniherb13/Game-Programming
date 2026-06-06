#pragma once

#include <vector>

namespace cr {

void AddWhiteOutlineRing(std::vector<unsigned char>& rgba, int w, int h, int ringPx = 3);
void AddBoldWhiteOutline(std::vector<unsigned char>& rgba, int w, int h);
void AddExtraBoldWhiteOutline(std::vector<unsigned char>& rgba, int w, int h);

} // namespace cr
