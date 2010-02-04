#pragma once

#include <map>
#include <string>
#include <list>
#include <vector>

namespace Jacker {
    
//=============================================================================

template<typename Event_T>
class EventCollection : public std::multimap<int,Event_T> {
public:
    typedef std::multimap<int,Event_T> map;
    typedef Event_T Event;
    typedef EventCollection<Event> BaseClass;
    
    void add_event(const Event &event) {
        insert(typename map::value_type(event.key(),event));
    }
};

//=============================================================================

#define NOTE(N,OCTAVE) ((12*(OCTAVE))+(Note ## N))

enum {
    NoteC = 0,
    NoteCs = 1,
    NoteD = 2,
    NoteDs = 3,
    NoteE = 4,
    NoteF = 5,
    NoteFs = 6,
    NoteG = 7,
    NoteGs = 8,
    NoteA = 9,
    NoteAs = 10,
    NoteB = 11,
};

enum {
    ValueNone = -1,
    
    NoteOff = 255,
};

enum {
    ParamNote = 0,
    ParamVolume,
    ParamCCIndex0,
    ParamCCValue0,
    ParamCCIndex1,
    ParamCCValue1,
    
    ParamCount,
};

//=============================================================================

// describes a parameter
struct Param {
    int min_value;
    int max_value;
    int def_value;
    int no_value;
    
    Param();
    Param(int min_value, int max_value, int default_value, int no_value);
};

//=============================================================================

struct PatternEvent {
    // time index
    int frame;
    // index of channel
    int channel;
    // index of parameter
    int param;
    // value of parameter
    int value;
    
    PatternEvent();
    PatternEvent(int frame, int channel, int param, int value);
    bool is_valid() const;
    int key() const;
    void sanitize_value();
};

//=============================================================================

struct Pattern : EventCollection<PatternEvent> {
    
    struct Row : std::vector<Event *> {
        typedef std::vector<Event *> vector;
        
        void resize(int channel_count);
        Event *get_event(int channel, int param);
        void set_event(Event &event);
    };
    
    // name of pattern (non-unique)
    std::string name;
    
    Pattern();
    
    void add_event(const Event &event);
    void add_event(int frame, int channel, int param, int value);
    
    void set_length(int length);
    int get_length() const;
    
    void set_channel_count(int count);
    int get_channel_count() const;

    void collect_events(int frame, iterator &iter, Row &row);
    iterator get_event(int frame, int channel, int param);
    
protected:
    // length in frames
    int length;
    // number of channels
    int channel_count;
};

//=============================================================================

struct TrackEvent {
    int frame;
    Pattern *pattern;
    
    int key() const;
};

struct Track : EventCollection<TrackEvent> {
    // name of track (non-unique)
    std::string name;
    // order of track
    int order;
    
    Track();
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
    Track &new_track();
};

//=============================================================================

} // namespace Jacker