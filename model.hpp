
#include <map>

namespace jacker {

enum {
    FRAMES_PER_BAR = 7680,
};

struct MIDIMessage {
	union {
		struct {
			union {
				struct {
					unsigned char channel:4;
					unsigned char command:4;
				};
				unsigned char status;
			};
			unsigned char data1;
			unsigned char data2;
			unsigned char rest;
		};
		long data;
		unsigned char bytes[4];
	};

	MIDIMessage() { 
        data = 0; 
    }

	bool operator ==(const MIDIMessage &other) const {
        if (data != other.data)
            return false;
        return true;
    }
    
	bool operator !=(const MIDIMessage &other) const {
        return !(*this == other);
    }
};

template<OwnerT>
struct EventMap {
    typedef std::multimap<int,OwnerT::Event> EventMap;
    
    // map of time -> event
    EventMap events;
};
    
struct Pattern : EventMap<Pattern> {
    struct Event {
        int frame;
        MIDIMessage midi_message;
    };
    
    // name of pattern (non-unique)
    std::string name;
    
    // loop cue in frames
    int loop_cue;
};

struct Track : EventMap<Pattern> {
    struct Event {
        int frame;
        Pattern *pattern;
    };
    
    // name of track (non-unique)
    std::string name;
};

struct Model {
    // list of all patterns
    std::list<Pattern*> patterns;
    
    // list of all tracks
    std::list<Track*> tracks;
    
    // end cue in frames
    int end_cue;
};

} // namespace jacker