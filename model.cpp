#include "model.hpp"
#include <cassert>
#include <stdio.h>

namespace Jacker {

//=============================================================================

static const char *note_strings[] = {
    "C-",
    "C#",
    "D-",
    "D#",
    "E-",
    "F-",
    "F#",
    "G-",
    "G#",
    "A-",
    "A#",
    "B-",
};

int sprint_note(char *buffer, int value) {
    if (value == ValueNone) {
        sprintf(buffer, "...");
    } else if (value == NoteOff) {
        sprintf(buffer, "===");
    } else {
        int note = value % 12;
        int octave = value / 12;
        
        assert((size_t)note < (sizeof(note_strings)/sizeof(const char *)));
        sprintf(buffer, "%s%i", note_strings[note], octave);
    }
    return 3;
}

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
            value = std::min(std::max(value,0),119);
        } break;
        case ParamCommand:
        {
            value = std::min(std::max(value,0x21),0x7f);
        } break;
        case ParamValue:
	{
	    value = std::min(std::max(value,0),0xff);
	} break;
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
    length = 1;
    channel_count = 1;
    refcount = 0;
}

Pattern::iterator Pattern::add_event(const Event &event) {
    assert(event.is_valid());
    assert(event.frame < length);
    assert(event.channel < channel_count);
    assert(event.param < ParamCount);
    iterator iter = get_event(event.frame, event.channel, event.param);
    if (iter != end()) {
        // replace event
        iter->second.value = event.value;
        return iter;
    } else {
        return BaseClass::add_event(event);
    }
}

Pattern::iterator Pattern::add_event(int frame, int channel, int param, int value) {
    return add_event(Event(frame,channel,param,value));
}

void Pattern::set_length(int length) {
    this->length = length;
    bool clipped = false;
    for (iterator iter = begin(); iter != end(); ++iter) {
        if (iter->second.frame >= length) {
            iter->second.frame = -1; // mark for delete
            clipped = true;
        }
    }
    if (clipped)
        update_keys();
}

int Pattern::get_length() const {
    return length;
}

void Pattern::set_channel_count(int count) {
    this->channel_count = std::min(std::max(count, 1), (int)MaxChannels);
    bool clipped = false;
    for (iterator iter = begin(); iter != end(); ++iter) {
        if (iter->second.channel >= channel_count) {
            iter->second.frame = -1; // mark for delete
            clipped = true;
        }
    }
    if (clipped)
        update_keys();
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

void Pattern::update_keys() {
    IterList dead_iters;
    EventList events;
    
    for (iterator iter = begin(); iter != end(); ++iter) {
        if (iter->first != iter->second.frame) {
            events.push_back(iter->second);
            dead_iters.push_back(iter);
        }
    }
    
    for (IterList::iterator iter = dead_iters.begin();
         iter != dead_iters.end(); ++iter) {
        erase(*iter);
    }
    
    for (EventList::iterator iter = events.begin();
         iter != events.end(); ++iter) {
        if (iter->frame == -1) // skip
            continue;
        add_event(*iter);
    }
}

void Pattern::copy_from(const Pattern &pattern) {
    name = pattern.name;
    length = pattern.length;
    channel_count = pattern.channel_count;
    
    const_iterator iter;
    for (iter = pattern.begin(); iter != pattern.end(); ++iter) {
        add_event(iter->second);
    }
}

//=============================================================================

SongEvent::SongEvent() {
    frame = ValueNone;
    pattern = NULL;
    track = 0;
    mute = false;
}

SongEvent::SongEvent(int frame, int track, Pattern &pattern) {
    this->frame = frame;
    this->track = track;
    this->pattern = &pattern;
}

int SongEvent::key() const {
    return frame;
}

int SongEvent::get_last_frame() const {
    return frame + pattern->get_length() - 1;
}

int SongEvent::get_end() const {
    return frame + pattern->get_length();
}

//=============================================================================

Song::Song() {
}

Song::iterator Song::add_event(const Event &event) {
    assert(event.pattern);
    event.pattern->refcount++;
    return BaseClass::add_event(event);
}

Song::iterator Song::add_event(int frame, int track, Pattern &pattern) {
    return add_event(Event(frame, track, pattern));
}

void Song::erase(iterator iter) {
    if (iter->second.pattern) {
        iter->second.pattern->refcount--;
    }
    BaseClass::erase(iter);
}

Song::iterator Song::get_event(int frame) {
    return BaseClass::find(frame);
}

void Song::find_events(int frame, IterList &events) {
    events.clear();
    Song::iterator iter;
    for (iter = begin(); iter != end(); ++iter) {
        if (iter->second.frame > frame)
            continue;
        if (iter->second.get_last_frame() < frame)
            continue;
        events.push_back(iter);
    }
}

void Song::update_keys() {
    IterList dead_iters;
    EventList events;
    
    for (iterator iter = begin(); iter != end(); ++iter) {
        if (iter->first != iter->second.frame) {
            events.push_back(iter->second);
            dead_iters.push_back(iter);
        }
    }
    
    for (IterList::iterator iter = dead_iters.begin();
         iter != dead_iters.end(); ++iter) {
        erase(*iter);
    }
    
    for (EventList::iterator iter = events.begin();
         iter != events.end(); ++iter) {
        if (iter->frame == -1) // skip
            continue;
        add_event(*iter);
    }
}

//=============================================================================

Measure::Measure() {
    bar = beat = subframe = 0;
}

void Measure::set_frame(Model &model, int frame) {
    int frames_per_bar = model.frames_per_beat * model.beats_per_bar;
    bar = frame / frames_per_bar;
    beat = (frame / model.frames_per_beat) % model.beats_per_bar;
    subframe = frame % model.frames_per_beat;
}

int Measure::get_frame(Model &model) const {
    return 0;
}

std::string Measure::get_string() const {
    char text[32];
    sprintf(text, "%i:%i.%i", bar, beat, subframe);
    return text;
}

//=============================================================================

Loop::Loop() {
    begin = end = 0;
}

void Loop::set(int begin, int end)  {
    this->begin = std::max(begin,0);
    this->end = std::max(end,0);
    if (this->begin > this->end)
        std::swap(this->begin, this->end);
}

void Loop::set_begin(int begin) {
    this->begin = std::max(begin,0);
    if (this->begin > this->end)
        this->end = this->begin;
}

void Loop::set_end(int end) {
    this->end = std::max(end,0);
    if (this->begin > this->end)
        this->begin = this->end;
}

void Loop::get(int &begin, int &end) const {
    begin = this->begin;
    end = this->end;
}

int Loop::get_begin() const {
    return begin;
}

int Loop::get_end() const {
    return end;
}

//=============================================================================

Track::Track() {
    midi_port = 0;
    midi_channel = 0;
    mute = false;
}

//=============================================================================

Model::Model() {
    reset();
}

void Model::reset() {
    end_cue = 0;
    midi_control_port = 0;
    midi_control_channel = 0;
    beats_per_minute = 120;
    frames_per_beat = 4;
    beats_per_bar = 4;
    enable_loop = true;
    loop.set(get_frames_per_bar()*4,get_frames_per_bar()*8);
    song.clear();
    tracks.clear();
    patterns.clear();
    
    tracks.resize(16);
    for (int i = 0; i < (int)tracks.size(); ++i) {
        tracks[i].midi_channel = std::min(i,15);
    }
}

Pattern &Model::new_pattern(const Pattern *template_pattern) {
    Pattern *pattern = new Pattern();
    if (template_pattern) {
        pattern->copy_from(*template_pattern);
    } else {
        char text[64];
        sprintf(text, "Pattern %i", patterns.size()+1);
        pattern->name = text;
        pattern->set_length(frames_per_beat * beats_per_bar);
    }
    patterns.push_back(pattern);
    return *pattern;
}

int Model::get_track_count() const {
    return tracks.size();
}

int Model::get_frames_per_bar() const {
    return frames_per_beat * beats_per_bar;
}

void Model::update_pattern_refcount() {
    // reset refcounts
    for (PatternList::iterator iter = patterns.begin(); 
         iter != patterns.end(); ++iter) {
        (*iter)->refcount = 0;
    }
    
    // recount references
    for (Song::iterator iter = song.begin(); iter != song.end(); ++iter) {
        iter->second.pattern->refcount++;
    }
}

void Model::delete_unused_patterns() {
    update_pattern_refcount();
    
    typedef std::list<PatternList::iterator> PatternIterList;
    PatternIterList dead_iters;
    
    for (PatternList::iterator iter = patterns.begin(); 
         iter != patterns.end(); ++iter) {
        //printf("pattern %s (%i)\n", (*iter)->name.c_str(), (*iter)->refcount);
        if ((*iter)->refcount <= 0) {
            dead_iters.push_back(iter);
        }
    }
    
    if (dead_iters.empty())
        return;
    printf("deleting %i unused patterns.\n", dead_iters.size());
    for (PatternIterList::iterator iter = dead_iters.begin(); 
         iter != dead_iters.end(); ++iter) {
        delete *(*iter);
        patterns.erase(*iter);
    } 
}

std::string Model::get_param_name(int param) const {
    switch(param) {
        case ParamNote: return "Note";
        case ParamVolume: return "Velocity";
        case ParamCommand: return "Command";
        case ParamValue: return "Argument";
        case ParamCCIndex: return "Control Id";
        case ParamCCValue: return "Control Value";
        default: break;
    };
    
    return "?";
}

std::string Model::format_param_value(int param, int value) const {
    if (value == ValueNone) {
        return "";
    }
    
    char text[64];    
    switch(param) {
        case ParamNote: {
            sprint_note(text, value);
        } break;
        case ParamCommand: {
            sprintf(text, "(Unknown)");
        } break;
        default: {
            sprintf(text, "%i", value);
        } break;
    };
    
    return text;
}

//=============================================================================

} // namespace Jacker