// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H
#define SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H

#include <stdexcept>
#include <mutex>

#include "axisrange.h"

namespace SimpleCairoPlot
{

class CircularBuffer
{
	std::mutex mtx;

	float* buf = NULL; float* bufend = NULL;
	float* end = NULL;
	unsigned int bufsize = 0, cnt = 0;
	unsigned long int cnt_overall = 0;
	
	// used for optimization of get_value_range()
	AxisRange range_abs_i_last_scan = AxisRange(-1, -1),
	          range_abs_i_min_max_last_scan = AxisRange(0, 0),
	          range_min_max_last_scan = AxisRange(0, 0);
	
	void copy_from(const CircularBuffer& from);
	float* get_item_addr(unsigned int i) const;
	
public:
	
	CircularBuffer(); void init(unsigned int sz); //init() must be called if this constructor is used
	CircularBuffer(unsigned int sz);
	CircularBuffer(const CircularBuffer& from);
	CircularBuffer& operator=(const CircularBuffer& buf);
	~CircularBuffer();
	
	unsigned int size() const;
	bool is_valid_range(AxisRange range) const;
	float& operator[](unsigned int i) const;
	
	unsigned int count() const;
	AxisRange range() const;
	bool is_full() const;
	unsigned long int count_overwriten() const;
	AxisRange range_to_abs(AxisRange range) const;
	AxisRange range_to_rel(AxisRange range_abs) const;
	
	void clear(bool clear_history_count = false);
	void push(float val);
	void load(const float* data, unsigned int cnt);
	
	AxisRange get_value_range(AxisRange range, unsigned int chk_step = 1);
};

inline float* CircularBuffer::get_item_addr(unsigned int i) const
{
	float* p;
	if (this->cnt < this->bufsize)
		p = this->buf + i;
	else
		p = this->end + i;
	
	if (p > this->bufend) p -= this->bufsize;
	return p;
}

inline unsigned int CircularBuffer::size() const
{
	return this->bufsize;
}

inline bool CircularBuffer::is_valid_range(AxisRange range) const
{
	return range.min() >= 0 && range.max() < this->bufsize;
}

inline float& CircularBuffer::operator[](unsigned int i) const
{
	if (i >= this->bufsize)
		throw std::out_of_range("CircularBuffer::operator[](): index exceeds the buffer size.");
	
	return *(this->get_item_addr(i));
}

inline unsigned int CircularBuffer::count() const
{
	return this->cnt;
}

inline AxisRange CircularBuffer::range() const
{
	return AxisRange(0, this->cnt - 1);
}

inline bool CircularBuffer::is_full() const
{
	return this->cnt == this->bufsize;
}

inline unsigned long int CircularBuffer::count_overwriten() const
{
	return this->cnt_overall - this->cnt;
}

inline AxisRange CircularBuffer::range_to_abs(AxisRange range) const
{
	AxisRange range_abs = range;
	range_abs.move(this->count_overwriten());
	return range_abs;
}

inline AxisRange CircularBuffer::range_to_rel(AxisRange range_abs) const
{
	AxisRange range = range_abs;
	range.move(-this->count_overwriten());
	return range;
}

inline void CircularBuffer::push(float val)
{
	if (! this->buf) return;
	
	*this->end = val;
	if (this->cnt < this->bufsize) this->cnt++;
	this->cnt_overall++;
	
	this->end++;
	if (this->end > this->bufend)
		this->end = this->buf;
}

}
#endif

