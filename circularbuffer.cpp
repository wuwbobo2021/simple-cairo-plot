// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#include <simple-cairo-plot/circularbuffer.h>

#include <cstring> //memcpy()
#include <limits> //numeric_limits<float>

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
	if (from.cnt == 0) return;
	
	this->lock(true);
	
	unsigned int cnt_cpy = from.cnt; //actual amount of data to be copied
	if (cnt_cpy > this->bufsize)
		cnt_cpy = this->bufsize;
	IndexRange range_cpy(from.count() - cnt_cpy, from.count() - 1);
	
	BufRangeMap map = from.map_from(range_cpy);
	memcpy(this->buf, from.buf + map.former.min(), map.former.count()*sizeof(float));
	if (map.latter)
		memcpy(this->buf + map.former.count(), from.buf + map.latter.min(),
		       map.latter.count()*sizeof(float));
	
	this->cnt = cnt_cpy;
	this->end = this->ptr_inc(this->buf, cnt_cpy);
	
	// copy the spike buffer only when both spike buffers have equal size
	if (from.bufsize == this->bufsize) {
		this->cnt_overwrite = from.cnt_overwrite + (from.cnt - cnt_cpy);
		memcpy(this->buf_spike, from.buf_spike,
			   this->buf_spike_size*sizeof(unsigned long int));
		this->spike_check_ref_min = from.spike_check_ref_min;
		this->buf_spike_cnt = from.buf_spike_cnt;
		this->buf_spike_end = from.buf_spike_end;
	}
	
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
		this->cnt_overwrite = 0;
	this->end = this->buf;
	this->buf[0] = 0;
	
	this->buf_spike_cnt = 0;
	this->buf_spike_end = this->buf_spike;
	this->spike_check_av = 0;
	
	this->last_min_max_scan = MinMaxScanInfo();
	this->last_av_calc = AvCalcInfo();
	
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
	if (data == NULL || cnt == 0) return;
	this->lock(true);
	
	unsigned int cnt_load = cnt; //actual amount of data to be read and loaded
	if (cnt_load > this->bufsize)
		cnt_load = this->bufsize;
	
	const float* pf, * pf_end; //pointer of data to be read
	pf_end = data + cnt - 1;
	pf = pf_end - cnt_load + 1;
	
	if (spike_check) {
		for (pf; pf <= pf_end; pf++)
			this->push(*pf, true, false);
		this->cnt_overwrite += cnt - cnt_load;
	} else {
		BufRangeMap map = this->map_from(IndexRange(0, cnt_load - 1));
		memcpy(this->buf + map.former.min(), pf, map.former.count()*sizeof(float));
		if (map.latter)
			memcpy(this->buf + map.latter.min(), pf + map.former.count(),
			       map.latter.count()*sizeof(float));
		
		unsigned long int tmp_cnt = this->cnt + cnt;
		if (tmp_cnt > this->bufsize) {
			this->cnt_overwrite += tmp_cnt - this->bufsize;
			this->cnt = this->bufsize;
		} else
			this->cnt = tmp_cnt;
	}
	
	this->unlock();
}

unsigned int CircularBuffer::get_spikes(IndexRange range, unsigned int* buf_out)
{
	if (this->buf_spike_cnt == 0) return 0;
	
	this->lock();
	range = this->range().cut_range(range);
	range = this->range_to_abs(range);
	
	unsigned int cnt_sp = 0;
	unsigned int* p = buf_out; unsigned long int cur;
	for (unsigned int i = 0; i < this->buf_spike_cnt; i++) {
		cur = this->buf_spike_item(i);
		if (cur < range.min()) continue;
		if (cur > range.max()) break;
		*p = this->index_to_rel(this->buf_spike_item(i));
		cnt_sp++; p++;
	}
	
	this->unlock();
	return cnt_sp;
}

unsigned int CircularBuffer::get_spikes(IndexRange range, unsigned long int* buf_out)
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

ValueRange CircularBuffer::get_value_range(IndexRange range, unsigned int chk_step)
{
	using std::numeric_limits;
	
	if (this->cnt == 0) return ValueRange(0, 0);
	
	// indexes used during the calculation are "absolute"
	range = this->range().cut_range(range);
	range = this->range_to_abs(range);
	if (chk_step == 0 || chk_step >= this->cnt / 2) chk_step = 1;
	
	MinMaxScanInfo last = this->last_min_max_scan;
	if (last.range_i_min_max_scan.contain(range) && range.contain(last.range_i_min_max))
		return last.range_min_max;
	
	this->lock();
	
	unsigned long int il = range.min(), ir = range.max();
	unsigned long int imin = il, imax = il;
	float min = numeric_limits<float>::max(), max = numeric_limits<float>::lowest();
	
	// optimized for scrolling right, but not for scrolling left
	if (range.intersected_not_left_of(last.range_i_min_max_scan)
	&&  range.contain(last.range_i_min_max))
	{
		imin = last.range_i_min_max.min(); min = last.range_min_max.min();
		imax = last.range_i_min_max.max(); max = last.range_min_max.max();
		il = last.range_i_min_max_scan.max();
	}
	
	// check for spikes
	unsigned long int i; float cur;
	if (chk_step > 1 && this->buf_spike_cnt > 0) {
		for (unsigned int i_sp = 0; i_sp < this->buf_spike_cnt; i_sp++) {
			i = this->buf_spike_item(i_sp);
			if (i < il) continue; if (i > ir) break;
			cur = this->abs_index_item(i);
			if (cur < min) {min = cur; imin = i;}
			if (cur > max) {max = cur; imax = i;}
		}
	}
	
	float* p = this->item_addr(this->index_to_rel(il));
	this->unlock();
	
	for (i = il; i <= ir; i += chk_step) {
		cur = *p;
		if (cur < min) {min = cur; imin = i;}
		if (cur > max) {max = cur; imax = i;}
		p = this->ptr_inc(p, chk_step);
	}
	
	last.range_i_min_max_scan = range;
	last.range_i_min_max.set(imin, imax);
	last.range_min_max.set(min, max);
	
	this->last_min_max_scan = last;
	return last.range_min_max;
}

float CircularBuffer::get_average(IndexRange range, unsigned int chk_step)
{
	if (this->cnt == 0) return 0;
	
	// indexes used during the calculation are "absolute"
	range = this->range().cut_range(range);
	range = this->range_to_abs(range);
	
	AvCalcInfo last = this->last_av_calc;
	if (range == last.range_i_av_val)
		return last.av_val;
	
	this->lock();
	
	bool flag_optimize = false, flag_add, flag_subtract;
	unsigned long int il_add, ir_add, il_sub, ir_sub; //index bounds
	unsigned int cnt = 0; float sum = 0;
	
	// optimized for scrolling right, but not for scrolling left
	if (last.range_i_av_val.min() > this->cnt_overwrite
	&&  range.intersected_not_left_of(last.range_i_av_val))
	{
		flag_add      = (range.max() > last.range_i_av_val.max());
		flag_subtract = (range.min() > last.range_i_av_val.min());
		
		unsigned int cnt_operate = 0;
		if (flag_add) {
			il_add = last.range_i_av_val.max() + 1; ir_add = range.max();
			cnt_operate += ir_add - il_add + 1;
		}
		if (flag_subtract) {
			il_sub = last.range_i_av_val.min(); ir_sub = range.min() - 1;
			cnt_operate += ir_sub - il_sub + 1;
		}
		flag_optimize = (cnt_operate < range.count());
	}
	
	if (! flag_optimize) {
		flag_add = true; flag_subtract = false;
		il_add = range.min(); ir_add = range.max();
	}
	
	if (chk_step == 0 || chk_step >= (ir_add - il_add + 1) / 32) {
		chk_step = (ir_add - il_add + 1) / 32; if (chk_step == 0) chk_step = 1;
	}
	
	if (flag_optimize) {
		cnt = last.range_i_av_val.count_by_step(chk_step);
		sum = cnt * last.av_val;
	}
	
	float* p_add, * p_sub, * p_add_end, * p_sub_end;
	if (flag_add) {
		unsigned int add_cnt = IndexRange(il_add, ir_add).count_by_step(chk_step);
		p_add = this->item_addr(this->index_to_rel(il_add));
		p_add_end = this->ptr_inc(p_add, (add_cnt - 1)*chk_step);
		cnt += add_cnt;
	}
	if (flag_subtract) {
		unsigned int sub_cnt = IndexRange(il_sub, ir_sub).count_by_step(chk_step);
		p_sub = this->item_addr(this->index_to_rel(il_sub));
		p_sub_end = this->ptr_inc(p_sub, (sub_cnt - 1)*chk_step);
		cnt -= sub_cnt;
	}
	
	this->unlock();
	
	if (flag_subtract) while (true) {
		sum -= *p_sub;
		if (p_sub == p_sub_end) break;
		p_sub = this->ptr_inc(p_sub, chk_step);
	}
	if (flag_add) while (true) {
		sum += *p_add;
		if (p_add == p_add_end) break;
		p_add = this->ptr_inc(p_add, chk_step);
	}
	
	last.av_val = sum / cnt;
	last.range_i_av_val = range;
	
	this->last_av_calc = last;
	return last.av_val;
}

