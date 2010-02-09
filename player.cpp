
#include "player.hpp"
#include "model.hpp"

namespace Jacker {

enum {
    // how many messages can be buffered?
    MaxMessageCount = 1024,
    // how many samples should be pre-mixed
    PreMixSize = 44100,
};

enum {
    CCVolume = 255,
};

//=============================================================================

Player::Channel::Channel() {
    note = ValueNone;
    volume = 0x7f;
}

//=============================================================================
    
Player::Bus::Bus() {
    channels.resize(MaxChannels);
}

//=============================================================================

Player::Message::Message() {
    timestamp = 0;
    frame = 0;
    bus = 0;
    bus_channel = 0;
}

Player::Player()
    : messages(MaxMessageCount),
      rt_messages(128) {
    buses.resize(MaxBuses);
    model = NULL;
    sample_rate = 44100;
    write_samples = 0;
    read_samples = 0;
    read_frame_block = 0;
    position = 0;
    read_position = 0;
    playing = false;
}

void Player::set_model(class Model &model) {
    this->model = &model;
}

void Player::set_sample_rate(int sample_rate) {
    this->sample_rate = sample_rate;
}

void Player::stop() {
    if (!playing)
        return;
    playing = false;
}

void Player::play() {
    if (playing)
        return;
    read_frame_block = 0;
    read_samples = 0;
    write_samples = 0;
    messages.clear();
    mix_events(PreMixSize);// fill buffer
    read_position = position;
    playing = true;
}

void Player::set_position(int position) {
    this->position = position;
}

int Player::get_position() const {
    return read_position;
}

void Player::reset() {
    stop();
}

long long Player::get_frame_size() {
    return ((long long)(sample_rate*60)<<32)/
           (model->frames_per_beat * model->beats_per_minute);    
}

void Player::on_cc(int ccindex, int ccvalue) {
    if (ccindex == ValueNone)
        return;
    if (ccvalue == ValueNone)
        return;
    Message msg;
    init_message(msg);
    msg.command = MIDI::CommandControlChange;
    msg.channel = 0;
    msg.data1 = ccindex;
    msg.data2 = ccvalue;
    messages.push(msg);
}

void Player::on_volume(int channel, int volume) {
    if (volume == ValueNone)
        return;
    Message msg;
    init_message(msg);
    msg.bus_channel = channel;
    msg.command = MIDI::CommandControlChange;
    msg.channel = 0;
    msg.data1 = CCVolume;
    msg.data2 = volume;
    messages.push(msg);    
}

void Player::on_note(int channel, int note, bool rt/*=false*/) {
    if (note == ValueNone)
        return;
    Message msg;
    init_message(msg);
    if (note == NoteOff) {
        msg.bus_channel = channel;
        msg.command = MIDI::CommandNoteOff;
        msg.channel = 0;
        msg.data1 = 0;
        msg.data2 = 0;
    } else {
        msg.bus_channel = channel;
        msg.command = MIDI::CommandNoteOn;
        msg.channel = 0;
        msg.data1 = note;
        msg.data2 = 0;
    }
    if (rt)
        rt_messages.push(msg);
    else
        messages.push(msg);
}

void Player::play_event(const class PatternEvent &event) {
    if (event.param != ParamNote)
        return;
    int note = event.value;
    if (note == ValueNone)
        return;
    on_note(event.channel, note, true);
}

void Player::mix_events(int samples) {
    assert(model);
    
    long long target = read_samples + ((long long)samples<<32);
    long long framesize = get_frame_size();
    int mixed = 0;
    while (write_samples < target)
    {
        mix_frame();
        position++;
        write_samples += framesize;
        mixed++;
    }
}

void Player::mix() {
    if (!playing)
        return;
    mix_events(PreMixSize);
}

void Player::init_message(Message &msg) {
    msg.timestamp = write_samples;
    msg.frame = position;
}

void Player::mix_frame() {
    assert(model);
    
    Song::EventList events;
    model.song.find_events(position, events);
    if (events.empty())
        return;
    
    Track::Event &event = iter->second;
    Pattern &pattern = *event.pattern;
    Pattern::iterator row_iter = pattern.begin();
    Pattern::Row row;
    pattern.collect_events(position - event.frame, row_iter, row);
    
    // first run: process all cc events
    for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
        on_cc(row.get_value(channel, ParamCCIndex), 
              row.get_value(channel, ParamCCValue));
    }
    
    // second run: process volume and notes
    for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
        on_volume(channel, row.get_value(channel, ParamVolume));
        on_note(channel, row.get_value(channel, ParamNote));
    }
}

void Player::handle_message(Message msg) {
    Bus &bus = buses[0];
    Channel &values = bus.channels[msg.bus_channel];
    
    if (msg.command == MIDI::CommandControlChange) {
        if (msg.data1 == CCVolume) {
            values.volume = msg.data2;
        } else {
            on_message(msg);
        }
        return;
    } else if (msg.command == MIDI::CommandNoteOff) {
        if (values.note != ValueNone) {
            msg.data1 = values.note;
            msg.data2 = 0;
            values.note = ValueNone;
            on_message(msg);            
        }
        return;
    } else if (msg.command == MIDI::CommandNoteOn) {
        if (values.note != ValueNone) {
            Message off_msg(msg);
            off_msg.command = MIDI::CommandNoteOff;
            off_msg.data1 = values.note;
            off_msg.data2 = 0;
            values.note = ValueNone;
            on_message(off_msg);
        }
        values.note = msg.data1;
        msg.data2 = values.volume;
        on_message(msg);
        return;
    }
}

void Player::process_messages(int _size) {
    long long size = (long long)_size << 32;
    long long offset = 0;
    
    Message next_msg;
    Message msg;
    while (size) {
        bool block_set = false;
        
        while (rt_messages.get_read_size()) {
            msg = rt_messages.pop();
            msg.timestamp = 0;
            handle_message(msg);
        }
        long long delta = size;
        
        if (messages.get_read_size()) {
            next_msg = messages.peek();
            delta = std::min(
                next_msg.timestamp - read_samples,size);
            if (delta < 0) {
                // drop
                messages.pop();
                delta = 0;
            } if (delta < size) {
                msg = messages.pop();
                read_position = msg.frame;
                read_frame_block = 0;
                block_set = true;
                msg.timestamp = offset;
                handle_message(msg);
            }
        }
        
        if (playing) {
            read_samples += delta;
            read_frame_block += delta;
            long long framesize = get_frame_size();
            while (read_frame_block >= framesize) {
                read_position++;
                read_frame_block -= framesize;
            }
        } else {
            read_position = position;
        }
        
        size -= delta;
        offset += delta;
    }
}

//=============================================================================

} // namespace Jacker