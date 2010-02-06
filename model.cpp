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
    switch(param) {
        case ParamNote:
        {
            if (value == NoteOff)
                return;
            value = std::min(std::max(value,0),0x7f);
        } break;
        case ParamCommand:
        {
            value = std::min(std::max(value,0x21),0x7f);
        } break;
        case ParamValue:
        case ParamVolume:
        case ParamCCIndex:
        case ParamCCValue:        
        {
            value = std::min(std::max(value,0),0x7f);
        } break;
        default: break;
    }
}

//=============================================================================

Pattern::Event *Pattern::Row::get_event(int channel, int param) const {
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

int Pattern::Row::get_value(int channel, int param) const {
    Event *evt = get_event(channel, param);
    if (!evt)
        return ValueNone;
    return evt->value;
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

TrackEvent::TrackEvent() {
    frame = ValueNone;
    pattern = NULL;
}

TrackEvent::TrackEvent(int frame, Pattern &pattern) {
    this->frame = frame;
    this->pattern = &pattern;
}

int TrackEvent::key() const {
    return frame;
}

int TrackEvent::get_last_frame() const {
    return frame + pattern->get_length() - 1;
}

//=============================================================================

Track::Track() {
    order = -1;
}

void Track::add_event(const Event &event) {
    BaseClass::add_event(event);
}

void Track::add_event(int frame, Pattern &pattern) {
    add_event(Event(frame, pattern));
}

Track::iterator Track::get_event(int frame) {
    return BaseClass::find(frame);
}

//=============================================================================

TrackEventRef::TrackEventRef() {
    track = NULL;
}

TrackEventRef::TrackEventRef(Track &track, Track::iterator iter) {
    this->track = &track;
    this->iter = iter;
}

bool TrackEventRef::operator ==(const TrackEventRef &other) const {
    if (track != other.track)
        return false;
    if (iter != other.iter)
        return false;
    return true;
}

bool TrackEventRef::operator !=(const TrackEventRef &other) const {
    return !(*this == other);
}

bool TrackEventRef::operator <(const TrackEventRef &other) const {
    if (track->order < other.track->order)
        return true;
    if (iter->second.frame < other.iter->second.frame)
        return true;
    return false;
}

//=============================================================================

Model::Model() {
    end_cue = 0;
    beats_per_minute = 120;
    frames_per_beat = 4;
    beats_per_bar = 4;
}

Pattern &Model::new_pattern() {
    Pattern *pattern = new Pattern();
    patterns.push_back(pattern);
    return *pattern;
}

Track &Model::new_track() {
    Track *track = new Track();
    tracks.push_back(track);
    renumber_tracks();
    return *track;
}

void Model::renumber_tracks() {
    TrackArray::iterator iter;
    int index = 0;
    for (iter = tracks.begin(); iter != tracks.end(); ++iter) {
        (*iter)->order = index;
        index++;
    }
}

int Model::get_track_count() const {
    return tracks.size();
}

Track &Model::get_track(int track) {
    return *tracks[track];
}

//=============================================================================

} // namespace Jacker