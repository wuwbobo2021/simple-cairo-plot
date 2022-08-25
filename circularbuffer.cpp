// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#include <simple-cairo-plot/circularbuffer.h>

#include <limits>

using namespace SimpleCairoPlot;

void CircularBuffer::init(unsigned int sz)
{
	if (sz == 0)
		throw std::invalid_argument("CircularBuffer::init(): invalid buffer size 0.");
	
	this->mtx.lock();
	
	if (this->buf != NULL) {delete[] this->buf; this->buf = NULL;}
	if (this->buf_spike != NULL) {delete[] this->buf_spike; this->buf_spike = NULL;}
	
	this->bufsize = sz;
	this->buf_spike_size = this->bufsize / 10 + 1;
	
	bool except_caught = false;
	try {
		this->buf_spike = new unsigned long int[this->buf_spike_size];
		this->buf = new float[this->bufsize];
	} catch (std::bad_alloc) {
		except_caught = true;
	}
	if (except_caught || this->buf == NULL || this->buf_spike == NULL) {
		if (this->buf_spike) {delete[] this->buf_spike; this->buf_spike = NULL;}
		throw std::bad_alloc();
	}
	
	for (unsigned int i = 0; i < this->bufsize; i++)
		this->buf[i] = 0;
	
	this->bufend = this->buf + this->bufsize - 1;
	this->buf_spike_bufend = this->buf_spike + this->buf_spike_size - 1;
	this->clear(true);
	
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
	
	this->clear(true);
	
	if (from.bufsize == this->bufsize) {
		for (unsigned int i = 0; i < this->bufsize; i++)
			this->buf[i] = from.buf[i];
		this->cnt = from.cnt;
		
		// copy the spike buffer only when both buffers have the same size
		this->spike_check_ref_min = from.spike_check_ref_min;
		for (unsigned int i = 0; i < this->buf_spike_size; i++)
			this->buf_spike[i] = from.buf_spike[i];
		this->buf_spike_cnt = from.buf_spike_cnt;
		this->buf_spike_end = this->buf_spike + this->buf_spike_cnt;
		if (this->buf_spike_end > this->buf_spike_bufend)
			this->buf_spike_end = this->buf_spike;
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
	
	this->end = this->buf + this->cnt;
	if (this->end > this->bufend)
		this->end = this->buf;
	
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
	if (clear_count_history)
		this->cnt_overall = 0;
	this->end = this->buf;
	this->buf[0] = 0;
	
	this->buf_spike_cnt = 0;
	this->buf_spike_end = this->buf_spike;
	this->stable_av = 0;
	
	this->range_abs_i_last_scan.set(-1, -1);
}

void CircularBuffer::erase()
{
	if (this->buf == NULL) return;
	
	this->mtx.lock();
	
	this->clear(true);
	for (unsigned int i = 0; i < this->bufsize; i++)
		this->buf[i] = 0;
	
	this->mtx.unlock();
}

void CircularBuffer::load(const float* data, unsigned int cnt, bool spike_check)
{
	if (cnt == 0) return;
	this->mtx.lock();
	
	const float* p;
	if (cnt <= this->bufsize)
		p = data;
	else
		p = data + cnt - this->bufsize;
	
	if (spike_check) {
		for (const float* pt = p; pt < p + cnt; pt++)
			this->push(*pt, true);
	} else {
		for (const float* pt = p; pt < p + cnt; pt++)
			this->push(*pt, false);
	}
	this->mtx.unlock();
}

unsigned int CircularBuffer::get_spikes(Range range, unsigned int* buf_out)
{
	if (this->buf_spike_cnt == 0) return 0;
	
	this->mtx.lock();
	range = this->range().cut_range(range);
	range = this->range_to_abs(range);
	
	unsigned int cnt_sp = 0; unsigned long int cur;
	for (unsigned int i = 0; i < this->buf_spike_cnt; i++) {
		cur = this->buf_spike_item(i);
		if (cur < range.min()) continue;
		if (cur > range.max()) break;
		
		*buf_out = this->buf_spike_item(i) - this->count_overwritten();
		cnt_sp++; buf_out++;
	}
	
	this->mtx.unlock();
	return cnt_sp;
}

float CircularBuffer::get_average(Range range, unsigned int chk_step) //not optimized
{
	if (this->cnt == 0) return 0;
	
	this->mtx.lock();
	
	range = this->range().cut_range(range);
	unsigned int il = range.min(), ir = range.max();
	if (chk_step == 0 || chk_step >= this->cnt / 2) chk_step = 1;
	
	float sum = 0;
	float* p = this->item_addr(il);
	for (unsigned int i = il; i <= ir; i += chk_step) {
		sum += *p;
		p += chk_step; if (p > this->bufend) p -= this->bufsize;
	}
	
	this->mtx.unlock();
	
	float cnt = (range.length() + 1.0) / chk_step;
	if (cnt > (unsigned int)cnt) cnt = (unsigned int)cnt + 1;
	return sum / cnt;
}

Range CircularBuffer::get_value_range(Range range, unsigned int chk_step)
{
	using std::numeric_limits;
	
	if (this->cnt == 0) return Range(0, 0);
	
	Range range_i_last_scan = this->range_to_rel(this->range_abs_i_last_scan),
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
	
	float* p = this->item_addr(il);
	for (unsigned int i = il; i <= ir; i += chk_step) {
		cur = *p;
		if (cur < min) {min = cur; imin = i;}
		if (cur > max) {max = cur; imax = i;}
		p += chk_step; if (p > this->bufend) p -= this->bufsize;
	}
	
	if (chk_step > 1 && this->buf_spike_cnt > 0) {
		for (unsigned int i_sp = 0, i; i_sp < this->buf_spike_cnt; i_sp++) {
			i = this->buf_spike_item(i_sp);
			if (i < il) continue; if (i > ir) break;
			cur = (*this)[i];
			if (cur < min) {min = cur; imin = i;}
			if (cur > max) {max = cur; imax = i;}
		}
	}
	
	this->range_abs_i_last_scan = this->range_to_abs(range);
	this->range_abs_i_min_max_last_scan = this->range_to_abs(Range(imin, imax));
	this->range_min_max_last_scan = Range(min, max);
	
	this->mtx.unlock();
	return this->range_min_max_last_scan;
}

