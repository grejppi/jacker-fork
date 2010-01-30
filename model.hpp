#pragma once

#include <map>
#include <string>
#include <list>

namespace Jacker {
    
//=============================================================================

template<typename Event_T>
struct EventCollection {
    typedef Event_T Event;
    typedef std::multimap<int,Event> EventMap;
    
    // map of time -> event
    EventMap events;
};

//=============================================================================

struct PatternEvent {
    enum {
        COMMAND_COUNT = 2,
    };
    
    struct Command {
        int cc;
        int value;
        
        Command();
    };
    
    int frame;
    int channel;
    int note;
    int volume;
    Command commands[COMMAND_COUNT];
    
    PatternEvent();
};

//=============================================================================

struct Pattern : EventCollection<PatternEvent> {
    // name of pattern (non-unique)
    std::string name;
    
    // length in frames
    int length;
    
    Pattern();
};

//=============================================================================

struct TrackEvent {
    int frame;
    Pattern *pattern;
};

struct Track : EventCollection<TrackEvent> {
    // name of track (non-unique)
    std::string name;
};

//=============================================================================

struct Model {
    // list of all patterns
    std::list<Pattern*> patterns;
    
    // list of all tracks
    std::list<Track*> tracks;
    
    // end cue in frames
    int end_cue;
    
    Model();
    Pattern &new_pattern();
};

//=============================================================================

} // namespace Jacker