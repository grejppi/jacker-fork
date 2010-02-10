
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

Message::Message() {
    type = TypeEmpty;
    timestamp = 0;
    frame = 0;
    bus = 0;
    bus_channel = 0;
}

//=============================================================================

MessageQueue::MessageQueue()
    : RingBuffer<Message>(MaxMessageCount) {
    write_samples = 0;
    position = 0;
    read_samples = 0;
}

void MessageQueue::init_message(Message &msg) {
    msg.timestamp = write_samples;
    msg.frame = position;
}

void MessageQueue::on_cc(int bus, int ccindex, int ccvalue) {
    if (ccindex == ValueNone)
        return;
    if (ccvalue == ValueNone)
        return;
    Message msg;
    init_message(msg);
    msg.type = Message::TypeMIDI;
    msg.bus = bus;
    msg.command = MIDI::CommandControlChange;
    msg.channel = 0;
    msg.data1 = ccindex;
    msg.data2 = ccvalue;
    push(msg);
}

void MessageQueue::on_volume(int bus, int channel, int volume) {
    if (volume == ValueNone)
        return;
    Message msg;
    init_message(msg);
    msg.type = Message::TypeMIDI;
    msg.bus = bus;
    msg.bus_channel = channel;
    msg.command = MIDI::CommandControlChange;
    msg.channel = 0;
    msg.data1 = CCVolume;
    msg.data2 = volume;
    push(msg);    
}

void MessageQueue::on_note(int bus, int channel, int note) {
    if (note == ValueNone)
        return;
    Message msg;
    init_message(msg);
    msg.type = Message::TypeMIDI;
    msg.bus = bus;
    msg.bus_channel = channel;
    if (note == NoteOff) {
        msg.command = MIDI::CommandNoteOff;
        msg.channel = 0;
        msg.data1 = 0;
        msg.data2 = 0;
    } else {
        msg.command = MIDI::CommandNoteOn;
        msg.channel = 0;
        msg.data1 = note;
        msg.data2 = 0;
    }
    push(msg);
}

void MessageQueue::status_msg() {
    Message msg;
    init_message(msg);
    msg.type = Message::TypeEmpty;
    push(msg);
}

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

Player::Player() {
    buses.resize(MaxTracks);
    model = NULL;
    sample_rate = 44100;
    read_position = 0;
    playing = false;
    front_index = 0;
}

MessageQueue &Player::get_back() {    
    return messages[(front_index+1)%QueueCount];
}

MessageQueue &Player::get_front() {
    return messages[front_index];
}

void Player::flip() {
    front_index = (front_index+1)%QueueCount;
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
    seek(read_position);
}

void Player::play() {
    if (playing)
        return;
    playing = true;
    seek(read_position);
}

bool Player::is_playing() const {
    return playing;
}

void Player::premix() {
    MessageQueue &queue = get_back();
    queue.clear();
    queue.read_samples = 0;
    queue.write_samples = 0;
    mix_events(queue, PreMixSize);// fill buffer
}

void Player::seek(int position) {
    MessageQueue &queue = get_back();
    queue.position = position;
    if (playing)
        premix();
    flip();
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

void Player::play_event(const class PatternEvent &event) {
    if (event.param != ParamNote)
        return;
    int note = event.value;
    if (note == ValueNone)
        return;
    rt_messages.on_note(0, event.channel, note);
}

void Player::mix_events(MessageQueue &queue, int samples) {
    assert(model);
    
    long long target = queue.read_samples + ((long long)samples<<32);
    long long framesize = get_frame_size();
    
    while (queue.write_samples < target)
    {
        // send status package
        queue.status_msg();
        mix_frame(queue);
        queue.write_samples += framesize;
        queue.position++;
        if (model->enable_loop && (queue.position == model->loop.get_end())) {
            queue.position = model->loop.get_begin();
        }
    }
}

void Player::mix() {
    if (!playing)
        return;
    mix_events(get_front(), PreMixSize);
}

void Player::mix_frame(MessageQueue &queue) {
    assert(model);
    
    Song::IterList events;
    model->song.find_events(queue.position, events);
    if (events.empty())
        return;
    
    Song::IterList::iterator iter;
    for (iter = events.begin(); iter != events.end(); ++iter) {
        Song::Event &event = (*iter)->second;
        Pattern &pattern = *event.pattern;
        Pattern::iterator row_iter = pattern.begin();
        Pattern::Row row;
        pattern.collect_events(queue.position - event.frame, row_iter, row);
        
        // first run: process all cc events
        for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
            queue.on_cc(event.track, row.get_value(channel, ParamCCIndex), 
                row.get_value(channel, ParamCCValue));
        }
        
        // second run: process volume and notes
        for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
            queue.on_volume(event.track, channel, 
                row.get_value(channel, ParamVolume));
            queue.on_note(event.track, channel, 
                row.get_value(channel, ParamNote));
        }
    }
    
}

void Player::handle_message(Message msg) {
    if (msg.type == Message::TypeEmpty) {
        // status package, discard
        return;
    }
    
    Bus &bus = buses[msg.bus];
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
    
    while (!rt_messages.empty()) {
        msg = rt_messages.pop();
        msg.timestamp = 0;
        handle_message(msg);
    }
    
    MessageQueue &queue = get_front();
    
    if (!playing) {
        read_position = queue.position;
        return;
    }
    
    while (size) {
        long long delta = size;
        
        if (!queue.empty()) {
            next_msg = queue.peek();
            long long offset = next_msg.timestamp - queue.read_samples;
            delta = std::min(offset, size);
            if (delta < 0) {
                // drop
                queue.pop();
                delta = 0;
            } if (delta < size) {
                msg = queue.pop();
                read_position = msg.frame;
                msg.timestamp = offset;
                handle_message(msg);
            }
        }

        queue.read_samples += delta;
        
        size -= delta;
        offset += delta;
    }
}

//=============================================================================

} // namespace Jacker