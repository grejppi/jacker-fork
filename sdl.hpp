
#include <SDL.h>

namespace SDL {

class GLApp {
public:
    GLApp();
    virtual ~GLApp();

    bool init();
    void quit();

    void main_loop();
    void exit();

    virtual bool render() { return false; }
    virtual void on_key_down(SDL_KeyboardEvent &event) {}
    virtual void on_mouse_motion(SDL_MouseMotionEvent &event) {}
    virtual void on_mouse_button_down(SDL_MouseButtonEvent &event) {}
    virtual void on_mouse_button_up(SDL_MouseButtonEvent &event) {}

protected:    
    bool exit_main_loop;
    int width;
    int height;

};    

}