#include "jack.hpp"

#include <stdio.h>
#include <assert.h>

namespace Jack {

//=============================================================================
    
static bool handle_status(int status)
{
	if (status == JackServerStarted) {
		printf("JACK: The JACK server was started as a result of this operation.\n");
        return true;
    }
    if (status & JackFailure)
		printf("JACK: Overall operation failed.\n");
	else if (status & JackInvalidOption)
		printf("JACK: The operation contained an invalid or unsupported option.\n");
	else if (status & JackNameNotUnique)
		printf("JACK: The desired client name was not unique.\n");
	else if (status & JackServerFailed)
		printf("JACK: Unable to connect to the JACK server.\n");
	else if (status & JackServerError)
		printf("JACK: Communication error with the JACK server.\n");
	else if (status & JackNoSuchClient)
		printf("JACK: Requested client does not exist.\n");
	else if (status & JackLoadFailure)
		printf("JACK: Unable to load internal client.\n");
	else if (status & JackInitFailure)
		printf("JACK: Unable to initialize client.\n");
	else if (status & JackShmFailure)
		printf("JACK: Unable to access shared memory.\n");
	else if (status & JackVersionError)
		printf("JACK: Client's protocol version does not match.\n");
	fflush(stdout);

	if (!status)
		return true;
	else
		return false;
}

//=============================================================================

int Client::process_callback(jack_nframes_t size, void *arg) {
    Client *client = (Client *)arg;
    for (PortList::iterator i = client->ports.begin(); i != client->ports.end(); ++i) {
        (*i)->update_buffer(size);
    }
    client->on_process(size);
    return 0;
}

int Client::sample_rate_callback(jack_nframes_t nframes, void *arg) {
    Client *client = (Client *)arg;
    client->on_sample_rate(nframes);
	return 0;
}

void Client::shutdown_callback(void *arg) {
    Client *client = (Client *)arg;
    client->on_shutdown();
}

Client::Client(const char *name) {
    this->name = name;
    handle = NULL;
    nframes = 0;
}

bool Client::is_created() {
    return handle != NULL;
}

Client::~Client() {
    assert(!handle); // you must call shutdown() before deleting the instance
    assert(ports.empty()); // you must delete all ports before deleting the client
}

bool Client::init() {
    assert(!handle); // you must call shutdown() before init
    
    jack_status_t status;
    handle = jack_client_open(name.c_str(),
                              JackNullOption,
                              &status);
    if (handle_status(status))
    {
		handle_status(jack_set_process_callback(
				handle, &process_callback, this));

		handle_status(jack_set_sample_rate_callback(
				handle, &sample_rate_callback, this));

		jack_on_shutdown(handle, &shutdown_callback, this);
        
        for (PortList::iterator i = ports.begin(); i != ports.end(); ++i) {
            (*i)->init();
        }
        
        return true;
    }
    
    return false;
}

void Client::activate() {
    jack_activate(handle);
}

void Client::deactivate() {
    jack_deactivate(handle);
}

void Client::shutdown() {
    assert(handle);
    
    for (PortList::iterator i = ports.begin(); i != ports.end(); ++i) {
        (*i)->shutdown();
    }
    handle_status(jack_client_close(handle));
    handle = NULL;
    
}

void Client::add_port(Port *port) {
    ports.push_back(port);
    if (handle) // port came after init, so post-init it
        port->init();
}

void Client::remove_port(Port *port) {
    if (port->handle) // port has not been shut down, so do it now
        port->shutdown();
    ports.remove(port);
}

//=============================================================================

Port::Port(Client &client, 
           const char *name, 
           const char *type, 
           unsigned long flags,
           unsigned long buffer_size) {
    handle = NULL;
    this->client = &client;
    this->name = name;
    this->type = type;
    this->flags = flags;
    this->buffer_size = buffer_size;

    this->client->add_port(this);
}

Port::~Port() {
    this->client->remove_port(this);
}

void Port::init() {
    assert(!handle);
    assert(client->handle);
    handle = jack_port_register(client->handle, 
                                name.c_str(), 
                                type.c_str(), 
                                flags, 
                                buffer_size);
}

void Port::shutdown() {
    assert(handle);
    assert(client->handle);
    handle_status(jack_port_unregister(client->handle, handle));
    handle = NULL;
}

void *Port::get_buffer(jack_nframes_t nframes) {
    return jack_port_get_buffer(handle, nframes);
}

//=============================================================================
    
AudioPort::AudioPort(Client &client, 
                     const char *name,
                     unsigned long flags)
 : Port(client, name, JACK_DEFAULT_AUDIO_TYPE, flags, 0) {
    
}

void AudioPort::update_buffer(jack_nframes_t nframes) {
    buffer = (DefaultAudioSample *)get_buffer(nframes);
}
    
//=============================================================================

MIDIPort::MIDIPort(Client &client, 
                   const char *name,
                   unsigned long flags) 
 : Port(client, name, JACK_DEFAULT_MIDI_TYPE, flags, 0) {
    
}

void MIDIPort::update_buffer(jack_nframes_t nframes) {
    buffer = get_buffer(nframes);
}

NFrames MIDIPort::get_event_count() {
    return jack_midi_get_event_count(buffer);
}

bool MIDIPort::get_event(MIDIEvent &event, NFrames index) {
    return (jack_midi_event_get(&event, buffer, index) == 0);
}

bool MIDIPort::get_event(MIDI::Message &msg, NFrames *time, NFrames index) {
    MIDIEvent event;
    if (get_event(event, index) && (event.size <= 3)) {
        if (time)
            *time = event.time;
        for (size_t i = 0; i < event.size; ++i) {
            msg.bytes[i] = event.buffer[i];
        }        
        return true;
    }
    return false;
}

void MIDIPort::clear_buffer() {
    jack_midi_clear_buffer(buffer);
}

size_t MIDIPort::max_event_size() {
    return jack_midi_max_event_size(buffer);
}

bool MIDIPort::write_event(NFrames time, MIDIData *data, size_t size) {
    return (jack_midi_event_write(buffer, time, data, size) == 0);
}

bool MIDIPort::write_event(NFrames time, const MIDI::Message &msg) {
    return write_event(time, (MIDIData*)&msg.data, 3);
}

NFrames MIDIPort::get_lost_event_count() {
    return jack_midi_get_lost_event_count(buffer);
}

MIDIData *MIDIPort::reserve_events(NFrames time, size_t size) {
    return jack_midi_event_reserve(buffer, time, size);
}

//=============================================================================

} // namespace Jack
