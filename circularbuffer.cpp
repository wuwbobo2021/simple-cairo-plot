// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include <limits>
#include <stdexcept>
#include "circularbuffer.h"

using namespace SimpleCairoPlot;

CircularBuffer::CircularBuffer(unsigned int sz):
	size(sz)
{
	if (sz == 0)
		throw std::invalid_argument("CircularBuffer::CircularBuffer(): invalid buffer size 0.");
	
	this->buf = new float[this->size];
	if (this->buf == NULL)
		throw std::runtime_error("CircularBuffer::CircularBuffer(): out of memory.");
	
	this->bufend = this->buf + this->size - 1;
	this->end = this->buf;
}

void CircularBuffer::copy_from(const CircularBuffer& from)
{
	this->mtx.lock();
	
	if (from.size == this->size) {
		for (unsigned int i = 0; i < this->size; i++)
			this->buf[i] = from.buf[i];
		this->cnt = from.cnt;
	}
	
	//the two conditions below should be avoided (not optimized)
	else if (from.size > this->size) {
		unsigned int offset = from.size - this->size;
		for (unsigned int i = 0; i < this->size; i++)
			this->buf[i] = from[offset + i];
		this->cnt = this->size;
	} else { //from.size < this->size
		for (unsigned int i = 0; i < from.size; i++)
			this->buf[i] = from[i];
		this->cnt = from.size;
	}
	
	this->mtx.unlock();
}

CircularBuffer::CircularBuffer(const CircularBuffer& from):
	size(from.size)
{
	this->buf = new float[from.size];
	this->copy_from(from);
}

CircularBuffer& CircularBuffer::operator=(const CircularBuffer& buf)
{
	this->copy_from(buf);
	return *this;
}

CircularBuffer::~CircularBuffer()
{
	delete[] this->buf;
}

AxisRange CircularBuffer::get_value_range(const AxisRange& range)
{
	if (this->cnt == 0) return AxisRange(0, 0);
	
	this->mtx.lock();
	unsigned int il = range.min(), ir = range.max();
	if (il >= this->cnt) il = this->cnt - 1; //rare
	if (ir >= this->cnt) ir = this->cnt - 1;
	
	using std::numeric_limits;
	float cur, min = numeric_limits<float>::max(), max = numeric_limits<float>::min();
	float* p = this->get_item_addr(il);
	for (unsigned int i = il; i <= ir; i++) {
		cur = *p;
		if (cur < min) min = cur;
		if (cur > max) max = cur;
		if (p < this->bufend) p++; else p = this->buf;
	}
	
	if (min > max)
		throw std::runtime_error("CircularBuffer::get_value_range(): min > max.");
	
	this->mtx.unlock();
	return AxisRange(min, max);
}

void CircularBuffer::clear(bool clear_count_history)
{
	this->cnt = 0;
	if (clear_count_history) this->cnt_overall = 0;
	this->end = this->buf;
}

void CircularBuffer::load(const float* data, unsigned int cnt) //TODO: some optimization
{
	if (cnt == 0) return;
	this->mtx.lock();
	
	const float* p;
	if (cnt <= this->size)
		p = data;
	else
		p = data + cnt - this->size;
	
	for (const float* pt = p; pt < p + cnt; pt++)
		this->push(*pt);
	
	this->mtx.unlock();
}

