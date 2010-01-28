
#include "sdl.hpp"

namespace SDL {

static bool handle_status(int status) {
    if (status < 0) {
        fprintf(stderr, "SDL: %s\n", SDL_GetError());
        return false;
    }
    return true;
}

template<typename T>
static bool handle_status(T *status) {
    return handle_status((status == NULL)?-1:0);
}
    

GLApp::GLApp() {
    exit_main_loop = false;
    width = 800;
    height = 600;
}

GLApp::~GLApp() {
}

bool GLApp::init() {
    
    if (handle_status(SDL_Init(SDL_INIT_VIDEO))) {
        
        SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
        
        SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
        SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
        
        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1); 
        //SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4); 
        
        Uint32 flags = SDL_OPENGL | SDL_RESIZABLE;// | SDL_FULLSCREEN;
        
        if (handle_status(SDL_SetVideoMode(width, height, 24, flags))) {
            return true;
        }
    }
    
    return false;
}

void GLApp::quit() {
    exit_main_loop = true;
}

void GLApp::main_loop() {
    SDL_Event event;
     
    exit_main_loop = false;
    while(!exit_main_loop) {
        while(SDL_PollEvent(&event)) {
            switch(event.type){
                case SDL_KEYDOWN:
                    on_key_down((SDL_KeyboardEvent &)event);
                    break;
                case SDL_MOUSEMOTION:
                    on_mouse_motion((SDL_MouseMotionEvent &)event);
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    on_mouse_button_down((SDL_MouseButtonEvent &)event);
                    break;
                case SDL_MOUSEBUTTONUP:
                    on_mouse_button_up((SDL_MouseButtonEvent &)event);
                    break;
                case SDL_QUIT:
                    quit();
                    break;
            }
        }
        
        if (render())
            SDL_GL_SwapBuffers();
        
        SDL_Delay(1);
    }
    
}

} // namespace SDL