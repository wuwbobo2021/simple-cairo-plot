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
	
	this->read_lock_counter = 0;
	this->lock(true);
	
	if (this->buf != NULL) {delete[] this->buf; this->buf = NULL;}
	if (this->buf_spike != NULL) {delete[] this->buf_spike; this->buf_spike = NULL;}
	
	this->bufsize = sz;
	this->buf_spike_size = this->bufsize / 32;
	if (this->buf_spike_size < 16) this->buf_spike_size = 16;
	
	bool except_caught = false;
	try {
		this->buf_spike = new unsigned long int[this->buf_spike_size];
		this->buf = new float[this->bufsize];
	} catch (std::bad_alloc) {
		except_caught = true;
	}
	if (except_caught || this->buf == NULL || this->buf_spike == NULL) {
		if (this->buf_spike) {delete[] this->buf_spike; this->buf_spike = NULL;}
		this->unlock(); throw std::bad_alloc();
	}
	
	for (unsigned int i = 0; i < this->bufsize; i++)
		this->buf[i] = 0;
	
	this->bufend = this->buf + this->bufsize - 1;
	this->buf_spike_bufend = this->buf_spike + this->buf_spike_size - 1;
	
	this->unlock();
	this->clear(true);
}

CircularBuffer::CircularBuffer() {}

CircularBuffer::CircularBuffer(unsigned int sz)
{
	this->init(sz);
}

void CircularBuffer::copy_from(const CircularBuffer& from)
{
	this->clear(true);
	this->lock(true);
	
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
	
	this->unlock();
}

CircularBuffer::CircularBuffer(CircularBuffer& from)
{
	this->init(from.bufsize);
	from.lock();
	this->copy_from(from);
	from.unlock();
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
	this->lock(true);
	
	this->cnt = 0;
	if (clear_count_history)
		this->cnt_overall = 0;
	this->end = this->buf;
	this->buf[0] = 0;
	
	this->buf_spike_cnt = 0;
	this->buf_spike_end = this->buf_spike;
	this->spike_check_av = 0;
	
	this->range_abs_i_last_scan.set(-1, -1);
	this->range_abs_i_last_av.set(-1, -1);
	
	this->unlock();
}

void CircularBuffer::erase()
{
	if (this->buf == NULL) return;
	this->clear(true);
	
	this->lock();
	for (unsigned int i = 0; i < this->bufsize; i++)
		this->buf[i] = 0;
	this->unlock();
}

void CircularBuffer::load(const float* data, unsigned int cnt, bool spike_check)
{
	if (cnt == 0) return;
	this->lock(true);
	
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
	this->unlock();
}

unsigned int CircularBuffer::get_spikes(Range range, unsigned int* buf_out)
{
	if (this->buf_spike_cnt == 0) return 0;
	
	this->lock();
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
	
	this->unlock();
	return cnt_sp;
}

unsigned int CircularBuffer::get_spikes(Range range, unsigned long int* buf_out)
{
	if (this->buf_spike_cnt == 0) return 0;
	
	this->lock();
	range = this->range().cut_range(range);
	range = this->range_to_abs(range);
	
	unsigned int cnt_sp = 0; unsigned long int cur;
	for (unsigned int i = 0; i < this->buf_spike_cnt; i++) {
		cur = this->buf_spike_item(i);
		if (cur < range.min()) continue;
		if (cur > range.max()) break;
		*buf_out = this->buf_spike_item(i);
		cnt_sp++; buf_out++;
	}
	
	this->unlock();
	return cnt_sp;
}

Range CircularBuffer::get_value_range(Range range, unsigned int chk_step)
{
	using std::numeric_limits;
	
	if (this->cnt == 0) return Range(0, 0);
	range = this->range().cut_range(range);
	if (chk_step == 0 || chk_step >= this->cnt / 2) chk_step = 1;
	
	Range range_i_last_scan = this->range_to_rel(this->range_abs_i_last_scan),
	      range_i_min_max_last_scan = this->range_to_rel(this->range_abs_i_min_max_last_scan);
	
	if (range_i_last_scan.contain(range) && range.contain(range_i_min_max_last_scan))
		return this->range_min_max_last_scan;
	
	this->lock();
	
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
	
	float* p = this->item_addr(il);
	for (unsigned int i = il; i <= ir; i += chk_step) {
		cur = *p;
		if (cur < min) {min = cur; imin = i;}
		if (cur > max) {max = cur; imax = i;}
		p += chk_step; if (p > this->bufend) p -= this->bufsize;
	}
	
	if (chk_step > 1 && this->buf_spike_cnt > 0) {
		for (unsigned int i_sp = 0, i; i_sp < this->buf_spike_cnt; i_sp++) {
			i = this->buf_spike_item(i_sp) - this->count_overwritten();
			if (i < il) continue; if (i > ir) break;
			cur = (*this)[i];
			if (cur < min) {min = cur; imin = i;}
			if (cur > max) {max = cur; imax = i;}
		}
	}
	
	Range range_min_max = Range(min, max);
	this->range_min_max_last_scan = range_min_max;
	this->range_abs_i_last_scan = this->range_to_abs(range);
	this->range_abs_i_min_max_last_scan = this->range_to_abs(Range(imin, imax));
	
	this->unlock();
	return range_min_max;
}

inline unsigned int div_ceil(unsigned int dividend, unsigned int divisor)
{
	unsigned int quo = dividend / divisor, rem = dividend % divisor;
	if (rem > 0) quo++; return quo;
}

float CircularBuffer::get_average(Range range, unsigned int chk_step)
{
	if (this->cnt == 0) return 0;
	range = this->range().cut_range(range);
	
	Range range_i_last_av = this->range_to_rel(this->range_abs_i_last_av);
	if (range == range_i_last_av) return this->last_av;
	
	this->lock();
	
	bool flag_optimize = false, flag_add, flag_subtract;
	unsigned int il, ir, sl, sr; //index bounds
	unsigned int cnt = 0; float sum = 0;
	
	if (range_i_last_av.contain(range.min()) && range.contain(range_i_last_av.max())) {
		flag_add      = (range.max() > range_i_last_av.max());
		flag_subtract = (range.min() > range_i_last_av.min());
		
		unsigned int cnt_operate = 0;
		if (flag_add) {
			il = range_i_last_av.max() + 1; ir = range.max();
			cnt_operate += ir - il + 1;
		}
		if (flag_subtract) {
			sl = range_i_last_av.min(); sr = range.min() - 1;
			cnt_operate += sr - sl + 1;
		}
		flag_optimize = (cnt_operate < range.length());
	}
	
	if (! flag_optimize) {
		flag_add = true; flag_subtract = false;
		il = range.min(); ir = range.max();
	}
	
	if (chk_step == 0 || chk_step >= (ir - il + 1) / 32) {
		chk_step = (ir - il + 1) / 32; if (chk_step == 0) chk_step = 1;
	}
	
	if (flag_optimize) {
		cnt = div_ceil(range_i_last_av.length() + 1, chk_step);
		sum = cnt * this->last_av;
	}
	
	if (flag_add) {
		float* p = this->item_addr(il);
		for (unsigned int i = il; i <= ir; i += chk_step) {
			sum += *p;
			p += chk_step; if (p > this->bufend) p -= this->bufsize;
		}
		cnt += div_ceil(ir - il + 1, chk_step);
	}
	if (flag_subtract) {
		float* p = this->item_addr(sl);
		for (unsigned int s = sl; s <= sr; s += chk_step) {
			sum -= *p;
			p += chk_step; if (p > this->bufend) p -= this->bufsize;
		}
		cnt -= div_ceil(sr - sl + 1, chk_step);
	}
	
	float av = sum / cnt;
	this->last_av = av;
	this->range_abs_i_last_av = this->range_to_abs(range);
	
	this->unlock();
	return av;
}

