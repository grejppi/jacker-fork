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
    
    long long write_samples; // 0-32: subsample, 32-64: sample
    int read_samples;
    int position; // in frames

    Player();
    void reset(class Model &model);
    void mix(class Model &model);
    void mix_track(class Model &model, class Track &track);
    int process(int size, Message &msg);
    
    void set_sample_rate(int sample_rate);
    
protected:
    int sample_rate;

};

//=============================================================================

} // namespace Jacker