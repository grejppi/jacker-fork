#pragma once

namespace Jacker {
    
//=============================================================================

class Drag {
public:
    int start_x, start_y;
    int x, y;
    int threshold;

    Drag();
    void start(int x, int y);
    void update(int x, int y);
    // returns true if the threshold has been reached
    bool threshold_reached();
    void get_delta(int &delta_x, int &delta_y);
    void get_rect(int &x, int &y, int &w, int &h);
};

//=============================================================================

} // namespace Jacker