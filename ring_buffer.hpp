#pragma once

#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <vector>

template<typename T>
class RingBuffer {
public:
	std::vector<T> buffer;
	volatile size_t write_ptr;
	volatile size_t read_ptr;
	volatile size_t written;
	volatile size_t read_count;

    RingBuffer() {
        buffer.resize(10);
        clear();
    }

	RingBuffer(int size) {
		buffer.resize(size);
		clear();
	}
    
    
    void resize(int size) {
        buffer.resize(size);
        clear();
    }
    
	virtual ~RingBuffer() {
	}

	void clear(bool wipe=true) {
        read_ptr = 0;
		write_ptr = 0;
		written = 0;
		read_count = 0;
        if (wipe)
            memset(&buffer[0], 0, get_size() * sizeof(T));
	}

	size_t get_size() {
		return buffer.size();
	}

	bool empty() {
		return (get_read_size() == 0);
	}

	bool full() {
		return (get_write_size() == 0);
	}

	// returns how much elements can be read
	size_t get_read_size() {
		size_t rp = read_ptr;
		size_t wp = write_ptr;
		if (rp <= wp)
			return wp - rp;
		return ((wp + get_size()) - rp);
	}

	// returns how much elements can be written
	size_t get_write_size() {
		size_t rp = read_ptr;
		size_t wp = write_ptr;
		// if write pointer equals to read pointer,
		// the buffer is empty. if they don't, we subtract
		// one element to keep the write pointer behind the read pointer.
		if (wp == rp)
			return get_size()-1;
		else if (wp < rp)
			return rp - wp - 1;
		return ((rp + get_size()) - wp) - 1;
	}

	void push(const T& element) {
		write(&element, 1);
	}

	T pop() {
		T element;
		read(&element, 1);
		return element;
	}
    
    T peek() {
        T element;
        read(&element, 1, true);
        return element;
    }

	void reset_read() {
		read_ptr = write_ptr;
	}

	void read(T *data, size_t count, bool keep=false) {
		if (get_read_size() < count)
			return;
		assert(data);
		assert((count > 0)&&(count <= get_size()));
		if ((read_ptr + count) <= get_size()) { // can we do a linear write?
			memcpy(data, &buffer[read_ptr], count * sizeof(T));
		} else { // it wraps, do it in two blocks then
			size_t blocksize1 = get_size() - read_ptr;
			size_t blocksize2 = count - blocksize1;
			memcpy(data, &buffer[read_ptr], blocksize1 * sizeof(T));
			memcpy(data + blocksize1, &buffer[0], blocksize2 * sizeof(T));
		}
        if (!keep) {
            size_t new_read_ptr = (read_ptr + count) % get_size();
            read_ptr = new_read_ptr;
            read_count += count;
        }
	}

	void write(const T *data, size_t count) {
		if (get_write_size() < count) {
			fprintf(stderr, "RingBuffer: write overflow (%i < %i)\n", get_write_size(), count);
			fflush(stderr);
			return;
		}
		assert(data);
		assert((count > 0)&&(count <= get_size()));
		if ((write_ptr + count) <= get_size()) { // can we do a linear write?
			memcpy(&buffer[write_ptr], data, count * sizeof(T));
		} else { // it wraps, do it in two blocks then
			size_t blocksize1 = get_size() - write_ptr;
			size_t blocksize2 = count - blocksize1;
			memcpy(&buffer[write_ptr], data, blocksize1 * sizeof(T));
			memcpy(&buffer[0], data + blocksize1, blocksize2 * sizeof(T));
		}
		size_t new_write_ptr = (write_ptr + count) % get_size();
		write_ptr = new_write_ptr;
		written += count;
	}
};
