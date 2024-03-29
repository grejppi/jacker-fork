
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

Message::Message() {
    type = TypeEmpty;
    timestamp = 0;
    frame = 0;
    bus = 0;
    bus_channel = 0;
    port = 0;
}

//=============================================================================

MessageQueue::MessageQueue()
    : RingBuffer<Message>(MaxMessageCount) {
    write_samples = 0;
    position = 0;
    read_samples = 0;
    model = NULL;
}

void MessageQueue::set_model(Model &model) {
    this->model = &model;
}

void MessageQueue::init_message(int bus, Message &msg) {
    assert(model);
    msg.timestamp = write_samples;
    msg.frame = position;
    msg.port = model->tracks[bus].midi_port;
}

void MessageQueue::on_cc(int bus, int ccindex, int ccvalue) {
    if (ccindex == ValueNone)
        return;
    if (ccvalue == ValueNone)
        return;
    assert(model);
    Message msg;
    init_message(bus,msg);
    msg.type = Message::TypeMIDI;
    msg.bus = bus;
    msg.command = MIDI::CommandControlChange;
    msg.channel = model->tracks[bus].midi_channel;
    msg.data1 = ccindex;
    msg.data2 = ccvalue;
    push(msg);
}

void MessageQueue::on_command(int bus, int channel, Message::Type command, int value, int value2, int value3) {
    if (value == ValueNone)
        return;
    if (value2 == ValueNone)
        value2 = 0;
    if (value3 == ValueNone)
        value3 = 0;
    assert(model);
    Message msg;
    init_message(bus,msg);
    msg.type = command;
    msg.bus = bus;
    msg.bus_channel = channel;
    msg.status = value;
    msg.data1 = value2;
    msg.data2 = value3;
    push(msg);
}

void MessageQueue::on_note(int bus, int channel, int note, int velocity) {
    assert(model);
    Message msg;
    init_message(bus,msg);
    msg.type = Message::TypeMIDI;
    msg.bus = bus;
    msg.bus_channel = channel;
    msg.channel = model->tracks[bus].midi_channel;
    if (note == NoteOff) {
        msg.command = MIDI::CommandNoteOff;
        msg.data1 = 0;
        msg.data2 = 0;
    } else if (note == ValueNone) {
        if (velocity == ValueNone)
            return;
        // aftertouch
        msg.command = MIDI::CommandAftertouch;
        msg.data1 = 0;
        msg.data2 = velocity;
    } else {
        if (velocity == ValueNone)
            velocity = 0x7f;
        msg.command = MIDI::CommandNoteOn;
        msg.data1 = note;
        msg.data2 = velocity;
    }
    push(msg);
}

void MessageQueue::all_notes_off(int bus) {
    on_cc(bus, MIDI::ControllerAllNotesOff, 0);
}

void MessageQueue::status_msg() {
    Message msg;
    init_message(0,msg);
    msg.type = Message::TypeEmpty;
    push(msg);
}

//=============================================================================

Player::Channel::Channel() {
    note = ValueNone;
    volume = 1.0f;
}

//=============================================================================
    
Player::Bus::Bus() {
    channels.resize(MaxChannels);
    notes.resize(128);
    for (NoteArray::iterator iter = notes.begin(); 
         iter != notes.end(); ++iter) {
        *iter = -1;
    }
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
    rt_messages.set_model(model);
    for (int i = 0; i < QueueCount; ++i) {
        messages[i].set_model(model);
    }    
}

void Player::set_sample_rate(int sample_rate) {
    this->sample_rate = sample_rate;
}

void Player::stop() {
    if (!playing)
        return;
    playing = false;
    seek(read_position);
    for (size_t bus = 0; bus < model->tracks.size(); ++bus) {
        rt_messages.all_notes_off(bus);
    }    
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

void Player::flush() {
    seek(read_position);
}

void Player::seek(int position) {
    MessageQueue &queue = get_back();
    queue.position = position;
    if (playing)
        premix();
    else
        read_position = position;
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

void Player::play_event(int track, const class PatternEvent &event) {
    if (event.param != ParamNote)
        return;
    int note = event.value;
    if (note == ValueNone)
        return;
    rt_messages.on_note(track, event.channel, note, ValueNone);
}

void Player::stop_events(int track) {
    rt_messages.all_notes_off(track);
}

void Player::mix_events(MessageQueue &queue, int samples) {
    assert(model);
    
    long long target = queue.read_samples + ((long long)samples<<32);
    while (queue.write_samples < target)
    {
        // send status package
        queue.status_msg();
        mix_frame(queue);
        long long framesize = get_frame_size();
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
        
        if (model->tracks[event.track].mute)
            continue; // ignore event
        
        Pattern::iterator row_iter = pattern.begin();
        Pattern::Row row;
        pattern.collect_events(queue.position - event.frame, row_iter, row);
        
        // first run: process all cc events
        for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
            int command = row.get_value(channel, ParamCommand);
            int ccindex = row.get_value(channel, ParamCCIndex);
            int ccvalue = row.get_value(channel, ParamCCValue);
            if (command != ValueNone) {
                int value = row.get_value(channel, ParamValue);
                if (command == Message::TypeCommandTempo) {
                    if (value != ValueNone)
                        model->beats_per_minute = std::max(1,value);
                } else {
                    queue.on_command(event.track, channel, (Message::Type)command, 
                    value, ccindex, ccvalue);
                }
            }
            queue.on_cc(event.track, ccindex, ccvalue);
        }
        
        // second run: process volume and notes
        for (int channel = 0; channel < pattern.get_channel_count(); ++channel) {
            queue.on_note(event.track, channel, 
                row.get_value(channel, ParamNote),
            row.get_value(channel, ParamVolume));
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
    
    if (msg.type == Message::TypeMIDI) {
        if (msg.command == MIDI::CommandControlChange) {
            switch(msg.data1) {
                case MIDI::ControllerAllNotesOff:
                {
                    for (NoteArray::iterator iter = bus.notes.begin();
                         iter != bus.notes.end(); ++iter) {
                        *iter = -1;
                    }
                    on_message(msg);
                } break;
                default:
                {
                    on_message(msg);
                } break;
            }
            return;
        } else if (msg.command == MIDI::CommandAftertouch) {
            if (values.note != ValueNone) {
                // insert note and pass on
                msg.data1 = values.note;
                on_message(msg);
            } else {
                msg.command = MIDI::CommandChannelPressure;
                msg.data1 = msg.data2;
                msg.data2 = 0;
                on_message(msg);
            }
            return;
        } else if (msg.command == MIDI::CommandNoteOff) {
            if (values.note != ValueNone) {
                int note = values.note;
                values.note = ValueNone;
                // see if that note is actually being played
                // on our channel, if yes, kill it.
                if (bus.notes[note] == msg.bus_channel) {
                    bus.notes[note] = -1;
                    msg.data1 = note;
                    msg.data2 = 0;
                    on_message(msg);
                }
            }
            return;
        } else if (msg.command == MIDI::CommandNoteOn) {
            if (values.note != ValueNone) {
                int note = values.note;
                values.note = ValueNone;
                // no matter where the note is played, kill it.
                bus.notes[note] = -1;
                Message off_msg(msg);
                off_msg.command = MIDI::CommandNoteOff;
                off_msg.data1 = note;
                off_msg.data2 = 0;
                on_message(off_msg);
            }
            values.note = msg.data1;
            int volume = std::min((int)((float)(msg.data2) * values.volume), 0x7f);
            msg.data2 = volume;
            bus.notes[values.note] = msg.bus_channel;
            on_message(msg);
            return;
        }
    } else if (msg.type == Message::TypeCommandChannelVolume) {
        values.volume = std::min((float)(msg.status) / 0x7f, 1.0f);
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
            delta = std::min(next_msg.timestamp - queue.read_samples, size);
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