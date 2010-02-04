#include "model.hpp"
#include <cassert>

namespace Jacker {

//=============================================================================

Param::Param() {
    min_value = ValueNone;
    max_value = ValueNone;
    def_value = ValueNone;
    no_value = ValueNone;
}

Param::Param(int min_value, int max_value, int def_value, int no_value) {
    this->min_value = min_value;
    this->max_value = max_value;
    this->def_value = def_value;
    this->no_value = no_value;
}
    
//=============================================================================

PatternEvent::PatternEvent() {
    frame = ValueNone;
    channel = ValueNone;
    param = ValueNone;
    value = ValueNone;
}

PatternEvent::PatternEvent(int frame, int channel, int param, int value) {
    this->frame = frame;
    this->channel = channel;
    this->param = param;
    this->value = value;
}

bool PatternEvent::is_valid() const {
    if (frame == ValueNone)
        return false;
    if (channel == ValueNone)
        return false;
    if (param == ValueNone)
        return false;
    if (value == ValueNone)
        return false;
    return true;
}

int PatternEvent::key() const {
    return frame;
}

void PatternEvent::sanitize_value() {
    if (value == ValueNone)
        return;
    if (param == ParamNote) {
        if (value == NoteOff)
            return;
    }
    if (value < 0)
        value = 0;
    else if (value > 127)
        value = 127;
}

//=============================================================================

Pattern::Event *Pattern::Row::get_event(int channel, int param) {
    int index = (channel * ParamCount) + param;
    return (*this)[index];
}

void Pattern::Row::set_event(Event &event) {
    assert(event.is_valid()); // we should only have valid events
    int index = (event.channel * ParamCount) + event.param;
    (*this)[index] = &event;
}

void Pattern::Row::resize(int channel_count) {
    vector::resize(channel_count * ParamCount);
    for (size_t i = 0; i < size(); ++i) {
        (*this)[i] = NULL;
    }    
}

//=============================================================================

Pattern::Pattern() {
    length = 0;
    channel_count = 0;
}

void Pattern::add_event(const Event &event) {
    assert(event.is_valid());
    assert(event.frame < length);
    assert(event.channel < channel_count);
    assert(event.param < ParamCount);
    iterator iter = get_event(event.frame, event.channel, event.param);
    if (iter != end()) {
        // replace event
        iter->second.value = event.value;
    } else {
        BaseClass::add_event(event);
    }
}

void Pattern::add_event(int frame, int channel, int param, int value) {
    add_event(Event(frame,channel,param,value));
}

void Pattern::set_length(int length) {
    this->length = length;
}

int Pattern::get_length() const {
    return length;
}

void Pattern::set_channel_count(int count) {
    this->channel_count = count;
}

int Pattern::get_channel_count() const {
    return channel_count;
}

void Pattern::collect_events(int frame, iterator &iter, Row &row) {
     // resize and reset row
    row.resize(channel_count);
    
    // advance iterator to active row
    while ((iter != end()) && (iter->second.frame < frame))
        iter++;
    while ((iter != end()) && (iter->second.frame == frame)) {
        PatternEvent &event = iter->second;
        row.set_event(event);
        iter++;
    }
}

Pattern::iterator Pattern::get_event(int frame, int channel, int param) {
    iterator iter = lower_bound(frame);
    if (iter == end())
        return iter;
    while ((iter != end()) && (iter->second.frame == frame)) {
        if ((iter->second.channel == channel) && (iter->second.param == param)) {
            return iter;
        }
        iter++;
    }
    return end();
}

//=============================================================================

int TrackEvent::key() const {
    return frame;
}

//=============================================================================

Track::Track() {
    order = 0;
}

//=============================================================================

Model::Model() {
    end_cue = 0;
}

Pattern &Model::new_pattern() {
    Pattern *pattern = new Pattern();
    patterns.push_back(pattern);
    return *pattern;
}

Track &Model::new_track() {
    Track *track = new Track();
    tracks.push_back(track);
    return *track;
}

//=============================================================================

} // namespace Jacker