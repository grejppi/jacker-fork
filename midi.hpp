#pragma once

namespace MIDI {

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
			unsigned char rest;
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
