#pragma once

#include <map>
#include <string>
#include <list>
#include <vector>

namespace Jacker {
    
class Model;
    
//=============================================================================

#if defined(WIN32)
template<typename Key_T, typename T>
inline typename std::map<Key_T,T>::iterator extract_iterator(
    std::pair< typename std::map<Key_T,T>::iterator, bool> result) {
    return result.first;
}
#else
template<typename Key_T, typename T>
inline typename std::map<Key_T,T>::iterator extract_iterator(
    std::pair< typename std::map<const Key_T,T>::iterator, bool> result) {
    return result.first;
}
#endif

template<typename Key_T, typename T>
inline typename std::multimap<Key_T,T>::iterator extract_iterator(
    typename std::multimap<Key_T,T>::iterator result) {
    return result;
}

template<typename Map_T>
class EventCollection
    : public Map_T {
public:
    typedef Map_T map;
    typedef typename map::key_type Key;
    typedef typename map::mapped_type Event;
    typedef EventCollection<Map_T> BaseClass;
    typedef std::list<Event> EventList;
    typedef std::list<typename Map_T::iterator> IterList;
    
    typename map::iterator add_event(const Event &event) {
        return extract_iterator<typename map::key_type, Event>(
            insert(typename map::value_type(event.key(),event)));
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
    ParamCommand,
    ParamValue,
    ParamCCIndex,
    ParamCCValue,
    
    ParamCount,
};

enum {
    MaxTracks = 32,
    MaxChannels = 256,
    MaxPorts = 8,
};

//=============================================================================

int sprint_note(char *buffer, int value);

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
        Event *get_event(int channel, int param) const;
        void set_event(Event &event);
        int get_value(int channel, int param) const;
    };
    
    // name of pattern (non-unique)
    std::string name;
    // usage count of pattern
    int refcount;
    
    iterator add_event(const Event &event);
    iterator add_event(int frame, int channel, int param, int value);
    
    void set_length(int length);
    int get_length() const;
    
    void set_channel_count(int count);
    int get_channel_count() const;

    void collect_events(int frame, iterator &iter, Row &row);
    iterator get_event(int frame, int channel, int param);
    
    void update_keys();
    void copy_from(const Pattern &pattern);
    
    Pattern();
protected:
    // length in frames
    int length;
    // number of channels
    int channel_count;
};

//=============================================================================

struct SongEvent {
    int frame;
    int track;
    Pattern *pattern;
    
    SongEvent();
    SongEvent(int frame, int track, Pattern &pattern);
    
    int key() const;
    int get_last_frame() const;
    int get_end() const;
};

//=============================================================================

class Song : public EventCollection< std::multimap<int,SongEvent> > {
    friend class Model;
public:
    iterator add_event(const Event &event);
    iterator add_event(int frame, int track, Pattern &pattern);

    iterator get_event(int frame);
    void erase(iterator iter);

    void find_events(int frame, IterList &events);

    void update_keys();
protected:
    Song();
};

//=============================================================================

struct Measure {
    int bar;
    int beat;
    int subframe;
    
    Measure();
    void set_frame(Model &model, int frame);
    int get_frame(Model &model) const;
    std::string get_string() const;
};

//=============================================================================

class Loop {
public:
    Loop();
    void set(int begin, int end);
    void set_begin(int begin);
    void set_end(int end);

    void get(int &begin, int &end) const;
    int get_begin() const;
    int get_end() const;
protected:
    int begin;
    int end;
};

//=============================================================================

class Track {
public:
    int midi_port;
    int midi_channel;

    Track();
};


//=============================================================================

typedef std::list<Pattern*> PatternList;
typedef std::vector<Track> TrackArray;

class Model {
public:
    // list of all patterns
    PatternList patterns;

    // contains all song events
    Song song;

    // track infos
    TrackArray tracks;

    // describes the loop
    Loop loop;
    // tells if the loop is enabled
    bool enable_loop;
    
    // end cue in frames
    int end_cue;
    // how many frames are in one beat
    int frames_per_beat;
    // how many beats are in one bar
    int beats_per_bar;
    // how many beats are in one minute
    int beats_per_minute;
    
    // what port to use for the midi control
    int midi_control_port;

    void reset();
    
    Model();
    Pattern &new_pattern(const Pattern *template_pattern=NULL);
    
    int get_track_count() const;
        
    int get_frames_per_bar() const;
    
    void update_pattern_refcount();
    void delete_unused_patterns();
    
    std::string get_param_name(int param) const;
    std::string format_param_value(int param, int value) const;
};

//=============================================================================

} // namespace Jacker