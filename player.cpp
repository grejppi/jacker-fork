
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

void Player::on_cc(int ccindex, int ccvalue) {
    if (ccindex == ValueNone)
        return;
    if (ccvalue == ValueNone)
        return;
    int midi_channel = 0;
    
    Message msg;
    init_message(msg);
    msg.command = MIDI::CommandControlChange;
    msg.channel = midi_channel;
    msg.data1 = ccindex;
    msg.data2 = ccvalue;
    messages.push(msg);
}

void Player::on_volume(int channel, int volume) {
    if (volume == ValueNone)
        return;
    int midi_channel = 0;
    
    Message msg;
    init_message(msg);
    msg.bus_channel = channel;
    msg.command = MIDI::CommandControlChange;
    msg.channel = midi_channel;
    msg.data1 = CCVolume;
    msg.data2 = volume;
    messages.push(msg);    
}

void Player::on_note(int channel, int note) {
    if (note == ValueNone)
        return;
    int midi_channel = 0;
    
    if (note != NoteOff) {
        Message msg;
        init_message(msg);
        msg.bus_channel = channel;
        msg.command = MIDI::CommandNoteOff;
        msg.channel = midi_channel;
        msg.data1 = 0;
        msg.data2 = 0;
        messages.push(msg);
    }
    
    Message msg;
    init_message(msg);
    msg.bus_channel = channel;
    msg.command = MIDI::CommandNoteOn;
    msg.channel = midi_channel;
    msg.data1 = note;
    msg.data2 = 0;
    messages.push(msg);
}

void Player::play_event(const class PatternEvent &event) {
    if (event.param != ParamNote)
        return;
    int note = event.value;
    if (note == ValueNone)
        return;
    on_note(event.channel, note);
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
        long long delta = size;
        
        if (messages.get_read_size()) {
            next_msg = messages.peek();
            delta = std::min(std::max(
                next_msg.timestamp - read_samples,(long long)0L),size);
            if (delta < size) {
                msg = messages.pop();
                read_position = msg.frame;
                read_frame_block = 0;
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