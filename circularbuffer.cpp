// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include <limits>
#include <stdexcept>
#include "circularbuffer.h"

using namespace SimpleCairoPlot;

void CircularBuffer::init(unsigned int sz)
{
	this->mtx.lock();

	this->bufsize = sz;
	if (this->buf != NULL) delete[] this->buf;
	
	if (sz == 0)
		throw std::invalid_argument("CircularBuffer::init(): invalid buffer size 0.");
	
	this->buf = new float[this->bufsize];
	if (this->buf == NULL)
		throw std::runtime_error("CircularBuffer::init(): out of memory.");
	
	this->bufend = this->buf + this->bufsize - 1;
	this->end = this->buf;
	
	this->mtx.unlock();
}

CircularBuffer::CircularBuffer() {}

CircularBuffer::CircularBuffer(unsigned int sz)
{
	this->init(sz);
}

void CircularBuffer::copy_from(const CircularBuffer& from)
{
	this->mtx.lock();
	
	if (from.bufsize == this->bufsize) {
		for (unsigned int i = 0; i < this->bufsize; i++)
			this->buf[i] = from.buf[i];
		this->cnt = from.cnt;
	}
	else if (from.bufsize > this->bufsize) {
		unsigned int offset = from.bufsize - this->bufsize;
		for (unsigned int i = 0; i < this->bufsize; i++)
			this->buf[i] = from[offset + i];
		this->cnt = this->bufsize;
	} else { //from.bufsize < this->bufsize
		for (unsigned int i = 0; i < from.bufsize; i++)
			this->buf[i] = from[i];
		this->cnt = from.bufsize;
	}
	
	this->mtx.unlock();
}

CircularBuffer::CircularBuffer(const CircularBuffer& from)
{
	this->init(from.bufsize);
	this->copy_from(from);
}

CircularBuffer& CircularBuffer::operator=(const CircularBuffer& buf)
{
	this->copy_from(buf);
	return *this;
}

CircularBuffer::~CircularBuffer()
{
	if (this->buf != NULL) delete[] this->buf;
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

void CircularBuffer::load(const float* data, unsigned int cnt)
{
	if (cnt == 0) return;
	this->mtx.lock();
	
	const float* p;
	if (cnt <= this->bufsize)
		p = data;
	else
		p = data + cnt - this->bufsize;
	
	for (const float* pt = p; pt < p + cnt; pt++)
		this->push(*pt);
	
	this->mtx.unlock();
}

