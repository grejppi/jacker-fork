#include "drag.hpp"

#include <cmath>
#include <algorithm>

namespace Jacker {

//=============================================================================

Drag::Drag() {
    x = y = 0;
    start_x = start_y = 0;
    threshold = 8;
}

void Drag::start(int x, int y) {
    this->x = this->start_x = x;
    this->y = this->start_y = y;
}

void Drag::update(int x, int y) {
    this->x = x;
    this->y = y;
}

bool Drag::threshold_reached() {
    int delta_x;
    int delta_y;
    get_delta(delta_x, delta_y);
    float length = std::sqrt((float)(delta_x*delta_x + delta_y*delta_y));
    if ((int)(length+0.5) >= threshold)
        return true;
    return false;
}

void Drag::get_delta(int &delta_x, int &delta_y) {
    delta_x = (x - start_x);
    delta_y = (y - start_y);
}

void Drag::get_rect(int &x, int &y, int &w, int &h) {
    int x0,y0,x1,y1;
    x0 = this->start_x;
    y0 = this->start_y;
    x1 = this->x;
    y1 = this->y;
    if (x0 > x1)
        std::swap(x0,x1);
    if (y0 > y1)
        std::swap(y0,y1);
    x = x0;
    y = y0;
    w = std::abs(x1 - x0);
    h = std::abs(y1 - y0);
}

//=============================================================================

} // namespace Jacker
