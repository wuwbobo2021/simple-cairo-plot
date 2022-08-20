// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H
#define SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H

#include <stdexcept>
#include <mutex>

#include "axisrange.h" //<cmath> included

#ifndef __GNUC__ //in this case <cmath> functions are not built-in (not optimized)
	#ifndef fabs
		#define fabs(f) (((f) >= 0)? (f) : -(f)) //the precompiler replaces fabs() with this expression
	#else
		#define FABS_DEFINED_BEFORE_CIRCULAR_BUFFER_H
	#endif
#endif

namespace SimpleCairoPlot
{

class CircularBuffer
{
	std::mutex mtx;
	
	float* buf = NULL; float* bufend = NULL;
	float* end = NULL;
	unsigned int bufsize = 0, cnt = 0;
	unsigned long int cnt_overall = 0;
	
	// used for spike check
	float spike_check_ref_min = 0;
	unsigned long int* buf_spike = NULL, * buf_spike_bufend = NULL;
	unsigned long int* buf_spike_end = NULL;
	unsigned int buf_spike_size = 0, buf_spike_cnt = 0;
	float stable_av = 0;
	
	// used for optimization of get_value_range()
	Range range_abs_i_last_scan = Range(-1, -1),
	      range_abs_i_min_max_last_scan = Range(0, 0),
	      range_min_max_last_scan = Range(0, 0);
	
	void copy_from(const CircularBuffer& from);
	float* item_addr(unsigned int i) const;
	unsigned long int buf_spike_item(unsigned int i) const;
	void buf_spike_push(unsigned long int val);
	void spike_check();
	
public:
	
	CircularBuffer(); void init(unsigned int sz); //init() must be called if this constructor is used
	CircularBuffer(unsigned int sz);
	CircularBuffer(const CircularBuffer& from);
	CircularBuffer& operator=(const CircularBuffer& buf);
	~CircularBuffer();
	
	unsigned int size() const;
	unsigned int spike_buffer_size() const;
	bool is_valid_range(Range range) const;
	float& operator[](unsigned int i) const;
	float& last_item() const;
	
	unsigned int count() const;
	Range range() const;
	Range range_max() const;
	bool is_full() const;
	unsigned long int count_overwriten() const;
	unsigned long int count_overall() const;
	Range range_to_abs(Range range) const;
	Range range_to_rel(Range range_abs) const;
	
	void clear(bool clear_history_count = false);
	void erase();
	void push(float val, bool spike_check = true);
	void load(const float* data, unsigned int cnt, bool spike_check = true);
	
	void set_spike_check_ref_min(float val);
	unsigned int get_spikes(unsigned int* buf_out);
	unsigned int get_spikes(Range range, unsigned int* buf_out);
	
	// get_average() is not optimized; get_value_range() is optimized for range moving right.
	float get_average(unsigned int chk_step = 1);
	float get_average(Range range, unsigned int chk_step = 1);
	Range get_value_range(unsigned int chk_step = 1);
	Range get_value_range(Range range, unsigned int chk_step = 1);
};

inline float* CircularBuffer::item_addr(unsigned int i) const
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

inline bool CircularBuffer::is_valid_range(Range range) const
{
	return range.min() >= 0 && range.max() < this->bufsize;
}

inline float& CircularBuffer::operator[](unsigned int i) const
{
	if (i >= this->bufsize)
		throw std::out_of_range("CircularBuffer::operator[](): index exceeds the buffer size.");
	
	return *(this->item_addr(i));
}

inline float& CircularBuffer::last_item() const
{
	return (*this)[this->cnt - 1];
}

inline unsigned int CircularBuffer::count() const
{
	return this->cnt;
}

inline Range CircularBuffer::range() const
{
	if (this->cnt > 0)
		return Range(0, this->cnt - 1);
	else
		return Range(0, 0);
}

inline Range CircularBuffer::range_max() const
{
	return Range(0, this->bufsize - 1);
}

inline bool CircularBuffer::is_full() const
{
	return this->cnt == this->bufsize;
}

inline unsigned long int CircularBuffer::count_overwriten() const
{
	return this->cnt_overall - this->cnt;
}

inline unsigned long int CircularBuffer::count_overall() const
{
	return this->cnt_overall;
}

inline Range CircularBuffer::range_to_abs(Range range) const
{
	Range range_abs = range;
	range_abs.move(this->count_overwriten());
	return range_abs;
}

inline Range CircularBuffer::range_to_rel(Range range_abs) const
{
	Range range = range_abs;
	range.move(-this->count_overwriten());
	return range;
}

inline void CircularBuffer::push(float val, bool spike_check)
{
	if (! this->buf) return;
	
	*this->end = val;
	if (this->cnt < this->bufsize) this->cnt++;
	this->cnt_overall++;
	
	this->end++;
	if (this->end > this->bufend)
		this->end = this->buf;
	
	if (spike_check)
		this->spike_check();
}

inline unsigned int CircularBuffer::spike_buffer_size() const
{
	return this->buf_spike_size;
}

inline void CircularBuffer::spike_check()
{
	using namespace std;
	
	if (this->cnt == 1) this->stable_av = this->last_item();	
	if (this->cnt < 3) return;
	
	float ref = this->stable_av;
	if (fabs(ref) < spike_check_ref_min)
		ref = spike_check_ref_min;
	
	float dd =  this->last_item() - this->buf[this->cnt - 2]
	         - (this->buf[this->cnt - 2] - this->buf[this->cnt - 3]);
	
	if (stable_av != 0 && fabs(dd / ref) > 0.05)
		this->buf_spike_push(this->cnt_overall - 2);
	else
		this->stable_av = 0.9*this->stable_av + 0.1*this->last_item();
}

inline void CircularBuffer::set_spike_check_ref_min(float val)
{
	if (val < 0) val = -val;
	this->spike_check_ref_min = val;
}

inline unsigned int CircularBuffer::get_spikes(unsigned int* buf_out)
{
	return this->get_spikes(this->range(), buf_out);
}

inline unsigned long int CircularBuffer::buf_spike_item(unsigned int i) const
{
	unsigned long int* p;
	if (this->buf_spike_cnt < this->buf_spike_size)
		p = this->buf_spike + i;
	else
		p = this->buf_spike_end + i;
	
	if (p > this->buf_spike_bufend) p -= this->bufsize;
	return *p;
}

inline void CircularBuffer::buf_spike_push(unsigned long int val)
{
	*this->buf_spike_end = val;
	if (this->buf_spike_cnt < this->buf_spike_size)
		this->buf_spike_cnt++;
	
	this->buf_spike_end++;
	if (this->buf_spike_end > this->buf_spike_bufend)
		this->buf_spike_end = this->buf_spike;
}

inline float CircularBuffer::get_average(unsigned int chk_step)
{
	return this->get_average(this->range(), chk_step);
}

inline Range CircularBuffer::get_value_range(unsigned int chk_step)
{
	return this->get_value_range(this->range(), chk_step);
}

}

#ifndef __GNUC__
	#ifndef FABS_DEFINED_BEFORE_CIRCULAR_BUFFER_H
		#undef fabs
	#endif
#endif

#endif //SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H

