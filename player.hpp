#pragma once

#include <vector>
#include "midi.hpp"
#include "ring_buffer.hpp"

namespace Jacker {

//=============================================================================

class Player {
public:
    struct Channel {
        int note;
        int volume;
        
        Channel();
    };
    
    typedef std::vector<Channel> ChannelArray;
    
    struct Bus {
        ChannelArray channels;
        
        Bus();
    };
    
    struct Message : MIDI::Message {
        long long timestamp;
        int frame;
        int bus;
        int bus_channel;
        
        Message();
    };

    Player();
    void reset();
    void mix();
    void mix_track(class Track &track);
    void process_messages(int size);
    virtual void on_message(const Message &msg) {}
    
    void set_model(class Model &model);
    void set_sample_rate(int sample_rate);
    
    void stop();
    void play();
    void set_position(int position);
    int get_position() const;
    
    void play_event(const class PatternEvent &event);
    
protected:
    void init_message(Message &msg);
    void handle_message(Message msg);
    void on_note(int channel, int value);
    void on_volume(int channel, int value);
    void on_cc(int ccindex, int ccvalue);
    long long get_frame_size();

    int sample_rate;
    std::vector<Bus> buses;
    RingBuffer<Message> messages;
    RingBuffer<Message> rt_messages;
    class Model *model;
    
    long long write_samples; // 0-32: subsample, 32-64: sample
    long long read_samples;
    long long read_frame_block;
    int position; // in frames
    int read_position; // last read position, in frames
    bool playing;

};

//=============================================================================

} // namespace Jacker