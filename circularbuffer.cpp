// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include "circularbuffer.h"

#include <limits>

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

void CircularBuffer::clear(bool clear_count_history)
{
	this->cnt = 0;
	if (clear_count_history) this->cnt_overall = 0;
	this->end = this->buf;
	
	this->range_abs_i_last_scan.set(-1, -1);
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

AxisRange CircularBuffer::get_value_range(AxisRange range, unsigned int chk_step)
{
	using std::numeric_limits;
	
	if (this->cnt == 0) return AxisRange(0, 0);
	
	AxisRange range_i_last_scan = this->range_to_rel(this->range_abs_i_last_scan),
	          range_i_min_max_last_scan = this->range_to_rel(this->range_abs_i_min_max_last_scan);
	
	if (range_i_last_scan.contain(range) && range.contain(range_i_min_max_last_scan))
		return this->range_min_max_last_scan;
	
	this->mtx.lock();
	
	range = this->range().cut_range(range);
	unsigned int il = range.min(), ir = range.max();
	
	unsigned int imin = il, imax = il;
	float cur, min = numeric_limits<float>::max(), max = numeric_limits<float>::lowest();
	
	if (range_i_last_scan.contain(range.min()) && range.contain(range_i_last_scan.max())
	&&  range.contain(range_i_min_max_last_scan))
	{
		imin = range_i_min_max_last_scan.min(); min = this->range_min_max_last_scan.min();
		imax = range_i_min_max_last_scan.max(); max = this->range_min_max_last_scan.max();
		il = range_i_last_scan.max();
	}
	
	if (chk_step == 0 || chk_step >= this->cnt / 2) chk_step = 1;
	
	float* p = this->get_item_addr(il);
	for (unsigned int i = il; i <= ir; i += chk_step) {
		cur = *p;
		if (cur < min) {min = cur; imin = i;}
		if (cur > max) {max = cur; imax = i;}
		p += chk_step; if (p > this->bufend) p -= this->bufsize;
	}
	
	this->range_abs_i_last_scan = this->range_to_abs(range);
	this->range_abs_i_min_max_last_scan = this->range_to_abs(AxisRange(imin, imax));
	this->range_min_max_last_scan = AxisRange(min, max);
	
	this->mtx.unlock();
	return this->range_min_max_last_scan;
}

