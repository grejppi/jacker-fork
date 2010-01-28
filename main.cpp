
#include "jack.hpp"

#include <gtkmm.h>
#include <assert.h>
#include <stdio.h>

#ifdef max
#undef max
#endif

namespace Jacker {

class Client : public Jack::Client {
public:
    Jack::MIDIPort *midi_omni_out;

    Client()
        : Jack::Client("jacker") {
        midi_omni_out = new Jack::MIDIPort(*this, "omni", Jack::MIDIPort::IsOutput);
    }
    
    ~Client() {
        delete midi_omni_out;
    }

    virtual void on_process(Jack::NFrames size) {
    }
};
    
} // namespace Jacker

int main(int argc, char **argv) {
    Gtk::Main kit(argc, argv);
    
    Jacker::Client client;
    
    printf("init client...\n");
    if (client.init()) {
        
        Glib::RefPtr<Gtk::Builder> builder = 
            Gtk::Builder::create_from_file("jacker.glade");
        
        Gtk::Window* window = 0;
        builder->get_widget("main", window);
        
        window->show_all();
        
        printf("activate client...\n");
        client.activate();
        
        kit.run(*window);
            
        printf("deactivate...\n");
        client.deactivate();
        
        printf("shutdown...\n");
        client.shutdown();
    }
    
    printf("bye.\n");
    
    return 0;
}

#ifdef WIN32
int WINAPI WinMain(      
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR lpCmdLine,
    int nCmdShow
) {
    return main(__argc, __argv);
}
#endif