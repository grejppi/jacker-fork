
#include "player.hpp"
#include "model.hpp"

namespace Jacker {

enum {
    // how many messages can be buffered?
    MaxMessageCount = 1024,
    // how many samples should be pre-mixed
    PreMixSize = 44100,
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
}

Player::Player() : messages(MaxMessageCount) {
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
    mix(); // fill buffer
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

void Player::mix() {
    if (!playing)
        return;
    assert(model);
    
    long long target = read_samples + ((long long)PreMixSize<<32);
    long long framesize = get_frame_size();
    int mixed = 0;
    while (write_samples < target)
    {
        TrackArray::iterator iter;
        for (iter = model->tracks.begin(); iter != model->tracks.end(); ++iter) {
            mix_track(*(*iter));
        }
        position++;
        write_samples += framesize;
        mixed++;
    }
    //printf("mixed %i frames\n", mixed);
}

void Player::init_message(Message &msg) {
    msg.timestamp = write_samples;
    msg.frame = position;
}

void Player::mix_track(Track &track) {
    assert(model);
    
    Track::iterator iter = track.find_event(position);
    if (iter == track.end())
        return;
    Track::Event &event = iter->second;
    Pattern &pattern = *event.pattern;
    Pattern::iterator row_iter = pattern.begin();
    Pattern::Row row;
    pattern.collect_events(position - event.frame, row_iter, row);
    
    Bus &bus = buses[0];
    int midi_channel = 0;
    
    // first run: process all cc events
    for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
        int ccindex = row.get_value(channel, ParamCCIndex);
        if (ccindex != ValueNone) {
            int ccvalue = row.get_value(channel, ParamCCValue);
            if (ccvalue != ValueNone) {
                Message msg;
                init_message(msg);
                msg.command = MIDI::CommandControlChange;
                msg.channel = midi_channel;
                msg.data1 = ccindex;
                msg.data2 = ccvalue;
                messages.push(msg);
            }
        }
    }
    
    // second run: process volume and notes
    for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
        Channel &values = bus.channels[channel];
        
        int volume = row.get_value(channel, ParamVolume);
        if (volume != ValueNone) {
            values.volume = volume;
        }
        int note = row.get_value(channel, ParamNote);
        if (note != ValueNone) {
            if (values.note != ValueNone) {
                Message msg;
                init_message(msg);
                msg.command = MIDI::CommandNoteOff;
                msg.channel = midi_channel;
                msg.data1 = values.note;
                msg.data2 = values.volume;
                messages.push(msg);
                
                values.note = ValueNone;
            }
            if (note != NoteOff) {
                Message msg;
                init_message(msg);
                msg.command = MIDI::CommandNoteOn;
                msg.channel = midi_channel;
                msg.data1 = note;
                msg.data2 = values.volume;
                messages.push(msg);
                
                values.note = note;
            }
        }
    }
}

int Player::process(int _size, Message &msg) {
    long long size = (long long)_size << 32;
    long long delta = size;
    
    if (messages.get_read_size()) {
        Message next_msg;
        next_msg = messages.peek();
        delta = std::min(std::max(
            next_msg.timestamp - read_samples,(long long)0L),(long long)size);
        if (delta < size) {
            msg = messages.pop();
            read_position = msg.frame;
            read_frame_block = 0;
            msg.timestamp = 0;
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
    
    return (int)(delta>>32);
}

//=============================================================================

} // namespace Jacker