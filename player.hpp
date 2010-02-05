#pragma once

#include <vector>
#include "midi.hpp"
#include "ring_buffer.hpp"

namespace Jacker {

//=============================================================================

class Player {
public:
    struct Bus {
        std::vector<int> notes;
        
        Bus();
    };
    
    std::vector<Bus> buses;
    
    struct Message : MIDI::Message {
        int timestamp;
        
        Message();
    };

    RingBuffer<Message> messages;
    int write_samples;
    int read_samples;

    Player();
    void mix(class Model &model);
    void mix_track(class Model &model, class Track &track);
    int process(int size, Message &msg);
};

//=============================================================================

} // namespace Jacker