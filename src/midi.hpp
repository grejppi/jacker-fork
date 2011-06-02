#pragma once

namespace MIDI {

//=============================================================================

// implemented after http://www.midi.org/techspecs/midimessages.php
enum {
    // channel voice messages
    // sent with channel number
    CommandNoteOff = 0x8, // note | velocity
    CommandNoteOn = 0x9, // note | velocity
    CommandAftertouch = 0xa, // note | pressure
    CommandControlChange = 0xb, // controller | value
                                // CC 120-127 is channel mode
    CommandProgramChange = 0xc, // program
    CommandChannelPressure = 0xd, // value
    CommandPitchWheel = 0xe, // 14-bit: 7-bit LSB | 7-bit MSB
    
    // channel mode controllers
    ControllerAllSoundOff = 120, // 0
    ControllerResetAll = 121, // 0
    ControllerLocalControl = 122, // 0 = off, 127 = on
    ControllerAllNotesOff = 123, // 0
    ControllerOmniModeOff = 124, // 0
    ControllerOmniModeOn = 125, // 0
    ControllerPolyOff = 126, // 0, number of channels
    ControllerPolyOn = 127, // 0
    
    // system common messages
    StatusSysEx = 0xf0,
    StatusSongPosition = 0xf2, // 14-bit: 7-bit LSB | 7-bit MSB
    StatusSongSelect = 0xf3, // song
    StatusTuneRequest = 0xf6,
    StatusSysExEnd = 0xf7,
    
    // system real-time messages
    StatusTimingClock = 0xf8,
    StatusStart = 0xfa,
    StatusContinue = 0xfb,
    StatusStop = 0xfc,
    StatusActiveSensing = 0xfe,
    StatusReset = 0xff,
};
    
//=============================================================================

struct Message {
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
			unsigned char data3; // usually unused
		};
		long data;
		unsigned char bytes[4];
	};
    
	Message() {
        data = 0; 
    }

	bool operator ==(const Message &other) const {
        if (data != other.data)
            return false;
        return true;
    }
    
	bool operator !=(const Message &other) const {
        return !(*this == other);
    }
};

//=============================================================================

} // namespace MIDI
