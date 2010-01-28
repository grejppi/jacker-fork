#pragma once

#include <jack/jack.h>
#include <jack/midiport.h>

#include <string>
#include <list>

namespace Jack {

typedef jack_nframes_t NFrames;

class Client;
    
//=============================================================================

class Port {
friend class Client;
public:
    enum {
        IsInput = JackPortIsInput|JackPortIsTerminal,
        IsOutput = JackPortIsOutput|JackPortIsTerminal,
    };

    Port(Client &client, 
         const char *name, 
         const char *type, 
         unsigned long flags,
         unsigned long buffer_size);
    
    ~Port();

protected:
    virtual void update_buffer(jack_nframes_t nframes) = 0;
    void *get_buffer(jack_nframes_t nframes);
    void init();
    void shutdown();

    Client *client;
    jack_port_t *handle;
    std::string name; 
    std::string type;
    unsigned long flags;
    unsigned long buffer_size;
};

//=============================================================================

typedef jack_default_audio_sample_t DefaultAudioSample;

class AudioPort : public Port {
public:
    DefaultAudioSample *buffer;

    AudioPort(Client &client, 
              const char *name,
              unsigned long flags);
    
protected:
    virtual void update_buffer(jack_nframes_t nframes);
};

//=============================================================================

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

//=============================================================================

typedef jack_midi_event_t MIDIEvent;
typedef jack_midi_data_t MIDIData;

class MIDIPort : public Port {
public:
    MIDIPort(Client &client, 
             const char *name,
             unsigned long flags);

    // midi input methods

    NFrames get_event_count();
    bool get_event(MIDIEvent &event, NFrames index);
    bool get_event(MIDIMessage &msg, NFrames *time, NFrames index);

    // midi output methods

    void clear_buffer();
    size_t max_event_size();
    bool write_event(NFrames time, MIDIData *data, size_t size);
    bool write_event(NFrames time, const MIDIMessage &msg);
    NFrames get_lost_event_count();
    MIDIData *reserve_events(NFrames time, size_t size);

protected:
    virtual void update_buffer(jack_nframes_t nframes);

    void *buffer;
};

//=============================================================================

class Client {
    friend class Port;

public:
    Client(const char *name);
    virtual ~Client();

    bool init();
    void shutdown();

    void activate();
    void deactivate();

    virtual void on_process(NFrames size) {}
    virtual void on_sample_rate(NFrames nframes) {}
    virtual void on_shutdown() {}
        
protected:
    void add_port(Port *port);
    void remove_port(Port *port);

    typedef std::list<Port *> PortList;
    PortList ports;
    std::string name;
    jack_client_t *handle;
    jack_nframes_t nframes;
    
    static int process_callback(jack_nframes_t size, void *arg);
    static int sample_rate_callback(jack_nframes_t nframes, void *arg);
    static void shutdown_callback(void *arg);
};

//=============================================================================

} // namespace Jack
