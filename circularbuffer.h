// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H
#define SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H

#include <limits>
#include <stdexcept>
#include <mutex>

#include "axisrange.h"

namespace SimpleCairoPlot
{

class CircularBuffer
{
	float* buf; float* bufend;
	float* end;
	unsigned int cnt = 0;
	unsigned long int cnt_overall = 0;
	
	std::mutex mtx;
	
	void copy_from(const CircularBuffer& from);
	float* get_item_addr(unsigned int i) const;
	
public:
	const unsigned int size;
	
	CircularBuffer(unsigned int sz);
	CircularBuffer(const CircularBuffer& from);
	CircularBuffer& operator=(const CircularBuffer& buf);
	~CircularBuffer();
	
	float& operator[](unsigned int i) const;
	
	unsigned int count() const;
	bool is_full() const;
	unsigned long int count_discarded() const; 
	bool is_valid_range(const AxisRange& range) const;
	AxisRange get_value_range(const AxisRange& range);
	
	void clear(bool clear_history_count = false);
	
	void push(float val);
	void load(const float* data, unsigned int cnt);
};

inline float* CircularBuffer::get_item_addr(unsigned int i) const
{
	float* p;
	if (this->cnt < this->size)
		p = this->buf + i;
	else
		p = this->end + i;
	
	if (p > this->bufend) p -= this->size;
	return p;
}

inline float& CircularBuffer::operator[](unsigned int i) const
{
	if (i >= this->size)
		throw std::out_of_range("CircularBuffer::operator[](): index exceeds the buffer size.");
	
	return *(this->get_item_addr(i));
}

inline unsigned int CircularBuffer::count() const
{
	return this->cnt;
}

inline unsigned long int CircularBuffer::count_discarded() const
{
	return this->cnt_overall - this->cnt;
}

inline bool CircularBuffer::is_full() const
{
	return this->cnt == this->size;
}

inline bool CircularBuffer::is_valid_range(const AxisRange& range) const
{
	return range.min() >= 0 && range.max() < this->size;
}

inline void CircularBuffer::push(float val)
{
	*this->end = val;
	if (this->cnt < this->size) this->cnt++;
	this->cnt_overall++;
	
	this->end++;
	if (this->end > this->bufend)
		this->end = this->buf;
}

}
#endif

