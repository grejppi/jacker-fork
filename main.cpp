
#include "jack.hpp"

#include <assert.h>

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
    
    Jacker::Client client;
    
    if (!client.init())
        return 0;
    
    client.activate();
    
    sleep(10);
    
    client.deactivate();

    client.shutdown();
    
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