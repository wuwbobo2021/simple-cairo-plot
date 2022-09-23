// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#ifndef SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H
#define SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H

#include <stdexcept>
#include <thread> //this_thread::sleep_for()
#include <atomic> //atomic_flag, atomic_uint

#include <simple-cairo-plot/axisrange.h> //<cmath> included

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
	float* buf = NULL; float* bufend = NULL;
	float* end = NULL;
	unsigned int bufsize = 0, cnt = 0;
	volatile unsigned long int cnt_overwrite = 0;
	
	// used for spike check
	float spike_check_ref_min = 0;
	unsigned long int* buf_spike = NULL, * buf_spike_bufend = NULL;
	unsigned long int* buf_spike_end = NULL;
	unsigned int buf_spike_size = 0, buf_spike_cnt = 0;
	float spike_check_av = 0;
	
	// used for optimization (indexes are "absolute")
	struct MinMaxScanInfo {
		Range range_i_min_max_scan = Range(-1, -1),
		      range_i_min_max = Range(0, 0), //two indexes stored as a range for convenience
		      range_min_max = Range(0, 0);
	};
	struct AvCalcInfo {
		Range range_i_av_val = Range(-1, -1);
		float av_val = 0;
	};
	std::atomic<MinMaxScanInfo> last_min_max_scan;
	std::atomic<AvCalcInfo> last_av_calc;
	
	// used to avoid multithreaded conflicts
	std::atomic_flag flag_lock = ATOMIC_FLAG_INIT; //atomic_flag is not implemented with mutex
	std::atomic_int read_lock_counter; //atomic_int is not implemented with mutex on most platforms
	
	void copy_from(const CircularBuffer& from);
	float* ptr_inc(float* p, unsigned int inc) const;
	float* item_addr(unsigned int i) const;
	unsigned long int buf_spike_item(unsigned int i) const;
	void buf_spike_push(unsigned long int val);
	void spike_check();
	
public:
	// locks for writing (except the constructor without parameter and the destructor)
	CircularBuffer(); void init(unsigned int sz); //init() must be called if this constructor is used
	CircularBuffer(unsigned int sz);
	CircularBuffer(CircularBuffer& from); //`from` is locked here for reading
	CircularBuffer(const CircularBuffer& from);
	CircularBuffer& operator=(const CircularBuffer& buf);
	~CircularBuffer();
	
	unsigned int size() const; unsigned int spike_buffer_size() const;
	bool is_valid_range(Range range) const;
	unsigned int count() const;
	Range range() const;
	Range range_max() const;
	bool is_full() const;
	unsigned long int count_overwritten() const;
	unsigned long int count_overall() const;
	
	unsigned long int index_to_abs(unsigned int i) const; //returns a fixed index after filling
	unsigned int index_to_rel(unsigned long int i) const; //turn back to "relative" index
	Range range_to_abs(Range range) const;
	Range range_to_rel(Range range_abs) const;
	
	float& item(unsigned int i) const;
	float& operator[](unsigned int i) const;
	float& abs_index_item(unsigned long int i) const;
	float& last_item() const;
	
	// locks for writing
	void clear(bool clear_history_count = false);
	void erase();
	void push(float val, bool spike_check = true);
	void load(const float* data, unsigned int cnt, bool spike_check = true);
	
	// get_spikes() locks for reading
	void set_spike_check_ref_min(float val);
	unsigned int get_spikes(unsigned int* buf_out);
	unsigned int get_spikes(Range range, unsigned int* buf_out);
	unsigned int get_spikes(Range range, unsigned long int* buf_out);
	
	// locks for reading; optimized for scrolling right
	Range get_value_range(unsigned int chk_step = 1);
	Range get_value_range(Range range, unsigned int chk_step = 1);
	float get_average(unsigned int chk_step = 1);
	float get_average(Range range, unsigned int chk_step = 1);
	
	// the buffer can be locked externally ONLY before writing to or reading multiple
	// data from the buffer through operator[]; member functions that lock for writing
	// should NOT be called inside that lock() and unlock() pair.
	void lock(bool for_writing = false);
	void unlock();
};

inline unsigned int CircularBuffer::size() const
{
	return this->bufsize;
}

inline unsigned int CircularBuffer::spike_buffer_size() const
{
	return this->buf_spike_size;
}

inline bool CircularBuffer::is_valid_range(Range range) const
{
	return range.min() >= 0 && range.max() < this->bufsize;
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

inline unsigned long int CircularBuffer::count_overwritten() const
{
	return this->cnt_overwrite;
}

inline unsigned long int CircularBuffer::count_overall() const
{
	return this->cnt + this->cnt_overwrite;
}

inline unsigned long int CircularBuffer::index_to_abs(unsigned int i) const
{
	return i + this->cnt_overwrite;
}

inline unsigned int CircularBuffer::index_to_rel(unsigned long int i) const
{
	unsigned long int cnt_ovr = this->cnt_overwrite;
	if (i >= cnt_ovr)
		return i - cnt_ovr;
	else
		return 0;
}

inline Range CircularBuffer::range_to_abs(Range range) const
{
	Range range_abs = range;
	range_abs.move(this->cnt_overwrite);
	return range_abs;
}

inline Range CircularBuffer::range_to_rel(Range range_abs) const
{
	Range range = range_abs;
	range.min_move_to(this->index_to_rel(range_abs.min()));
	return range;
}

inline float& CircularBuffer::item(unsigned int i) const
{
	if (i >= this->bufsize)
		throw std::out_of_range("CircularBuffer::operator[](): index exceeds the buffer size.");
	
	return *(this->item_addr(i));
}

inline float& CircularBuffer::operator[](unsigned int i) const
{
	return this->item(i);
}

inline float& CircularBuffer::abs_index_item(unsigned long int i) const
{
	return this->item(this->index_to_rel(i));
}

inline float& CircularBuffer::last_item() const
{
	return this->item(this->cnt - 1);
}

inline void CircularBuffer::push(float val, bool spike_check)
{
	if (! this->buf) return;
	this->lock(true);
	
	*this->end = val;
	if (this->cnt < this->bufsize)
		this->cnt++;
	else
		this->cnt_overwrite++;
	
	this->end++;
	if (this->end > this->bufend)
		this->end = this->buf;
	
	if (spike_check)
		this->spike_check();
	
	this->unlock();
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

inline Range CircularBuffer::get_value_range(unsigned int chk_step)
{
	return this->get_value_range(this->range(), chk_step);
}

inline float CircularBuffer::get_average(unsigned int chk_step)
{
	return this->get_average(this->range(), chk_step);
}

inline void CircularBuffer::lock(bool for_writing)
{
	using namespace std::chrono;
	using namespace std::this_thread;
	
	// write operation must wait for previous operation;
	// read operation must wait for previous write operation.
	if (this->flag_lock.test_and_set(std::memory_order_acquire)
	&& (for_writing || this->read_lock_counter == 0)) {
		unsigned char i = 0;
		while (this->flag_lock.test_and_set(std::memory_order_acquire)) {
			sleep_for(microseconds((int) pow(2, i))); if (i < 12) i++;
		}
	}
	
	if (! for_writing)
		++this->read_lock_counter; //read_lock_counter > 0: locked for reading
}

inline void CircularBuffer::unlock()
{
	// in case of two threads trying to unlock at the same time when the
	// counter's original value is 1, because read_lock_counter is atomic,
	// at least one of the two threads can fix the minus value problem.
	if (this->read_lock_counter.load(std::memory_order_acquire) > 0) {
		int counter = --this->read_lock_counter;
		if (counter > 0) return;
		if (counter < 0) this->read_lock_counter = 0;
	}
	this->flag_lock.clear(std::memory_order_release);
}

// private

inline float* CircularBuffer::ptr_inc(float* p, unsigned int inc) const
{
	p += inc;
	while (p > this->bufend)
		p -= this->bufsize;
	return p;
}

inline float* CircularBuffer::item_addr(unsigned int i) const
{
	if (this->cnt < this->bufsize)
		return this->ptr_inc(this->buf, i);
	else
		return this->ptr_inc(this->end, i);
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

inline void CircularBuffer::spike_check()
{
	using namespace std;
	
	if (this->cnt == 1) this->spike_check_av = this->last_item();	
	if (this->cnt < 3) return;
	
	float ref = this->spike_check_av;
	if (fabs(ref) < spike_check_ref_min)
		ref = spike_check_ref_min;
	
	float dd =  this->last_item() - this->buf[this->cnt - 2]
	         - (this->buf[this->cnt - 2] - this->buf[this->cnt - 3]);
	
	if (spike_check_av != 0 && fabs(dd / ref) > 0.05)
		this->buf_spike_push(this->count_overall() - 2);
	else
		this->spike_check_av = 0.9*this->spike_check_av + 0.1*this->last_item();
}

}

#ifndef __GNUC__
	#ifndef FABS_DEFINED_BEFORE_CIRCULAR_BUFFER_H
		#undef fabs
	#endif
#endif

#endif //SIMPLE_CAIRO_PLOT_CIRCULAR_BUFFER_H

