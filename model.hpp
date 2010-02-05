#pragma once

#include <map>
#include <string>
#include <list>
#include <vector>

#include "ring_buffer.hpp"
#include "midi.hpp"

namespace Jacker {
    
class Model;
    
//=============================================================================

template<typename Map_T>
class EventCollection
    : public Map_T {
public:
    typedef Map_T map;
    typedef typename map::mapped_type Event;
    typedef EventCollection<Map_T> BaseClass;
    
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

enum {
    MaxBus = 32,
    MaxChannels = 256,
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

class Pattern : public EventCollection< std::multimap<int,PatternEvent> > {
    friend class Model;
public:
    struct Row : std::vector<Event *> {
        typedef std::vector<Event *> vector;
        
        void resize(int channel_count);
        Event *get_event(int channel, int param);
        void set_event(Event &event);
    };
    
    // name of pattern (non-unique)
    std::string name;
    
    void add_event(const Event &event);
    void add_event(int frame, int channel, int param, int value);
    
    void set_length(int length);
    int get_length() const;
    
    void set_channel_count(int count);
    int get_channel_count() const;

    void collect_events(int frame, iterator &iter, Row &row);
    iterator get_event(int frame, int channel, int param);
    
protected:
    Pattern();
    
    // length in frames
    int length;
    // number of channels
    int channel_count;
};

//=============================================================================

struct TrackEvent {
    int frame;
    Pattern *pattern;
    
    TrackEvent();
    TrackEvent(int frame, Pattern &pattern);
    
    int key() const;
    int get_last_frame() const;
};

class Track : public EventCollection< std::map<int,TrackEvent> > {
    friend class Model;
public:
    // name of track (non-unique)
    std::string name;
    // order of track
    int order;
    
    void add_event(const Event &event);
    void add_event(int frame, Pattern &pattern);

    iterator get_event(int frame);
protected:
    Track();
};

//=============================================================================

struct TrackEventRef {
    Track *track;
    Track::iterator iter;
    
    TrackEventRef();
    TrackEventRef(Track &track, Track::iterator iter);
    
    bool operator ==(const TrackEventRef &other) const;
    bool operator !=(const TrackEventRef &other) const;
    bool operator <(const TrackEventRef &other) const;
};

//=============================================================================

typedef std::list<Pattern*> PatternList;
typedef std::vector<Track*> TrackArray;

struct Model {
    // list of all patterns
    PatternList patterns;
    
    // list of all tracks
    TrackArray tracks;
    
    // end cue in frames
    int end_cue;
    
    Model();
    Pattern &new_pattern();
    Track &new_track();
    void renumber_tracks();
    
    int get_track_count() const;
    Track &get_track(int track);
};

//=============================================================================

class Player {
public:
    struct Bus {
        int notes[MaxChannels];
        
        Bus();
    };

    RingBuffer<MIDI::Message> messages;
    int write_frame;
    int read_samples;

    Player();
    void mix(Model &model);
    void mix_track(Model &model, Track &track);
    bool process(unsigned int &size);
};

//=============================================================================

} // namespace Jacker