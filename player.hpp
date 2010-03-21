#pragma once

#include <vector>
#include "midi.hpp"
#include "ring_buffer.hpp"

namespace Jacker {

//=============================================================================

struct Message : MIDI::Message {
    enum Type {
        // empty, for updating position
        TypeEmpty = 0,
        // midi package
        TypeMIDI = 1,
    };
    
    Type type;
    long long timestamp;
    int frame;
    int bus;
    int bus_channel;
    int port;
    
    Message();
};

class MessageQueue : public RingBuffer<Message> {
public:
    MessageQueue();
    volatile long long write_samples; // 0-32: subsample, 32-64: sample
    volatile int position; // in frames
    volatile long long read_samples;

    void on_note(int bus, int channel, int value, int velocity=0x7f);
    void on_cc(int bus, int ccindex, int ccvalue);
    void all_notes_off(int bus);

    void status_msg();

    void init_message(int  bus, Message &msg);

    void set_model(class Model &model);
protected:
    class Model *model;
};
    
class Player {
public:
    enum {
        // how many message queues are used
        // for flipping?
        QueueCount = 4,
    };
    
    struct Channel {
        int volume;
        int note;
        
        Channel();
    };
    
    typedef std::vector<Channel> ChannelArray;
    typedef std::vector<char> NoteArray;
    
    struct Bus {
        ChannelArray channels;
        // stores which key is pressed on
        // which channel. (notes[key] = channel)
        NoteArray notes;
        
        Bus();
    };

    Player();
    void reset();
    void mix();
    void process_messages(int size);
    virtual void on_message(const Message &msg) {}
    
    void set_model(class Model &model);
    void set_sample_rate(int sample_rate);
    
    void stop();
    void play();
    void seek(int position);
    void flush();
    int get_position() const;
        
    bool is_playing() const;
    
    void play_event(int track, const class PatternEvent &event);
    
protected:
    void premix();
    void mix_events(MessageQueue &queue, int samples);
    void mix_frame(MessageQueue &queue);
    void handle_message(Message msg);
    long long get_frame_size();

    MessageQueue &get_back();
    MessageQueue &get_front();
    void flip();

    int sample_rate;
    volatile int front_index; // index of messages front buffer
    std::vector<Bus> buses;
    MessageQueue messages[QueueCount];
    MessageQueue rt_messages;
    class Model *model;
    
    volatile int read_position; // last read position, in frames
    volatile bool playing;

};

//=============================================================================

} // namespace Jacker