
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
}

Player::Player() : messages(MaxMessageCount) {
    buses.resize(MaxBuses);
    sample_rate = 44100;
    write_samples = 0;
    read_samples = 0;
    position = 0;
}

void Player::set_sample_rate(int sample_rate) {
    this->sample_rate = sample_rate;
}

void Player::reset(Model &model) {
    messages.clear();
    mix(model);
}

void Player::mix(Model &model) {
    int target = read_samples + PreMixSize;
    int mixed = 0;
    while ((write_samples>>32) < target)
    {
        TrackArray::iterator iter;
        for (iter = model.tracks.begin(); iter != model.tracks.end(); ++iter) {
            mix_track(model, *(*iter));
        }
        position++;
        write_samples += ((long long)(sample_rate*60)<<32)/
            (model.frames_per_beat * model.beats_per_minute);
        mixed++;
    }
    //printf("mixed %i frames\n", mixed);
}

void Player::mix_track(Model &model, Track &track) {
    Track::iterator iter = track.upper_bound(position);
    if (iter == track.begin())
        return;
    iter--;
    Track::Event &event = iter->second;
    if (event.get_last_frame() < position)
        return; // already ended
    Pattern &pattern = *event.pattern;
    Pattern::iterator row_iter = pattern.begin();
    Pattern::Row row;
    pattern.collect_events(position - event.frame, row_iter, row);
    
    int timestamp = (write_samples>>32);
    
    Bus &bus = buses[0];
    int midi_channel = 0;
    
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
                msg.timestamp = timestamp;
                msg.command = MIDI::CommandNoteOff;
                msg.channel = midi_channel;
                msg.data1 = values.note;
                msg.data2 = values.volume;
                messages.push(msg);
                
                values.note = ValueNone;
            }
            if (note != NoteOff) {
                Message msg;
                msg.timestamp = timestamp;
                msg.command = MIDI::CommandNoteOn;
                msg.channel = midi_channel;
                msg.data1 = note;
                msg.data2 = values.volume;
                messages.push(msg);
                
                values.note = note;
            }
        }
        int ccindex0 = row.get_value(channel, ParamCCIndex0);
        if (ccindex0 != ValueNone) {
            int ccvalue0 = row.get_value(channel, ParamCCValue0);
            if (ccvalue0 != ValueNone) {
                Message msg;
                msg.timestamp = timestamp;
                msg.command = MIDI::CommandControlChange;
                msg.channel = midi_channel;
                msg.data1 = ccindex0;
                msg.data2 = ccvalue0;
                messages.push(msg);
            }
        }
        int ccindex1 = row.get_value(channel, ParamCCIndex1);
        if (ccindex1 != ValueNone) {
            int ccvalue1 = row.get_value(channel, ParamCCValue1);
            if (ccvalue1 != ValueNone) {
                Message msg;
                msg.timestamp = timestamp;
                msg.command = MIDI::CommandControlChange;
                msg.channel = midi_channel;
                msg.data1 = ccindex1;
                msg.data2 = ccvalue1;
                messages.push(msg);
            }
        }
    }
}

int Player::process(int size, Message &msg) {
    int delta = size;
    if (messages.get_read_size()) {
        Message next_msg;
        next_msg = messages.peek();
        delta = std::min(std::max(next_msg.timestamp - read_samples,0),size);
        if (delta < size) {
            msg = messages.pop();
            msg.timestamp = 0;
        }
    }
    read_samples += delta;
    return delta;
}

//=============================================================================

} // namespace Jacker