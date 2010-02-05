
#include "player.hpp"
#include "model.hpp"

namespace Jacker {

//=============================================================================
    
Player::Bus::Bus() {
    notes.resize(MaxChannels);
}

Player::Message::Message() {
    timestamp = 0;
}

Player::Player() : messages(64) {
    buses.resize(MaxBuses);
    write_samples = 0;
    read_samples = 0;
}

void Player::mix(Model &model) {
    TrackArray::iterator iter;
    for (iter = model.tracks.begin(); iter != model.tracks.end(); ++iter) {
        mix_track(model, *(*iter));
    }
    write_samples++;
    if (write_samples == 64)
        write_samples = 0;    
}

void Player::mix_track(Model &model, Track &track) {
    Track::iterator iter = track.upper_bound(write_samples);
    if (iter == track.begin())
        return;
    iter--;
    Track::Event &event = iter->second;
    if (event.get_last_frame() < write_samples)
        return; // already ended
    Pattern &pattern = *event.pattern;
    Pattern::iterator row_iter = pattern.begin();
    Pattern::Row row;
    pattern.collect_events(write_samples - event.frame, row_iter, row);
    
    for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
        Pattern::Event *evt = row.get_event(channel, ParamNote);
        if (!evt)
            continue;
        Message msg;
        msg.command = 0x9;
        msg.data1 = evt->value;
        msg.data2 = 0x7f;
        messages.push(msg);
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