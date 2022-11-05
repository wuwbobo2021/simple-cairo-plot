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
class CircularBuffer; struct BufRangeMap;

// mapping from index range in the circular buffer to 1 or 2 segment(s) in memory
struct BufRangeMap {
	IndexRange former, latter;
	BufRangeMap();
	BufRangeMap(IndexRange range, unsigned int bufsize, unsigned int cur);
};

class CircularBuffer
{
public:
	// locks for writing (except the constructor without parameter and the destructor)
	CircularBuffer(); void init(unsigned int sz); //init() must be called if this constructor is used
	CircularBuffer(unsigned int sz);
	CircularBuffer(CircularBuffer& from); //`from` is locked here for reading
	CircularBuffer(const CircularBuffer& from);
	CircularBuffer& operator=(const CircularBuffer& buf);
	~CircularBuffer();
	
	unsigned int size() const; unsigned int spike_buffer_size() const;
	bool is_valid_range(IndexRange range) const;
	unsigned int count() const;
	IndexRange range() const;
	IndexRange range_max() const;
	bool is_full() const;
	unsigned long int count_overwritten() const;
	unsigned long int count_overall() const;
	
	unsigned long int index_to_abs(unsigned int i) const; //returns a fixed index after filling
	unsigned int index_to_rel(unsigned long int i) const; //turn back to "relative" index
	IndexRange range_to_abs(IndexRange range) const;
	IndexRange range_to_rel(IndexRange range_abs) const;
	
	float& item(unsigned int i) const;
	float& operator[](unsigned int i) const;
	float& abs_index_item(unsigned long int i) const;
	float& last_item() const;
	
	// locks for writing
	void clear(bool clear_history_count = false);
	void erase();
	void push(float val, bool spike_check = true, bool lock = true);
	void load(const float* data, unsigned int cnt, bool spike_check = true); //optimized without spike check
	
	// get_spikes() locks for reading
	void set_spike_check_ref_min(float val);
	unsigned int get_spikes(unsigned int* buf_out); //short naming, actually turning points
	unsigned int get_spikes(IndexRange range, unsigned int* buf_out);
	unsigned int get_spikes(IndexRange range, unsigned long int* buf_out);
	
	// locks for reading; optimized for scrolling right
	ValueRange get_value_range(unsigned int chk_step = 1);
	ValueRange get_value_range(IndexRange range, unsigned int chk_step = 1);
	float get_average(unsigned int chk_step = 1);
	float get_average(IndexRange range, unsigned int chk_step = 1);
	
	// the buffer can be locked externally ONLY before writing to or reading multiple
	// data from the buffer through operator[]; member functions that lock for writing
	// should NOT be called inside that lock() and unlock() pair.
	void lock(bool for_writing = false);
	void unlock();
	
private:
	float* buf = NULL; float* bufend = NULL;
	float* end = NULL; //points to where the next item should be stored in
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
		IndexRange range_i_min_max_scan,
		           range_i_min_max; //two indexes stored as a range for convenience
		ValueRange range_min_max = ValueRange(0, 0);
	};
	struct AvCalcInfo {
		IndexRange range_i_av_val;
		float av_val = 0;
	};
	std::atomic<MinMaxScanInfo> last_min_max_scan;
	std::atomic<AvCalcInfo> last_av_calc;
	
	// used to avoid multithreaded conflicts
	std::atomic_flag flag_lock = ATOMIC_FLAG_INIT; //atomic_flag is not implemented with mutex
	std::atomic_int read_lock_counter; //atomic_int is not implemented with mutex on most platforms
	
	void copy_from(const CircularBuffer& from);
	float* ptr_inc(float* p, unsigned int inc = 1) const;
	float* item_addr(unsigned int i) const;
	BufRangeMap map_from(IndexRange range) const;
	unsigned long int buf_spike_item(unsigned int i) const;
	void buf_spike_push(unsigned long int val);
	void spike_check();
};

inline BufRangeMap::BufRangeMap() {}

inline BufRangeMap::BufRangeMap(IndexRange range, unsigned int bufsize, unsigned int cur)
{
	if (bufsize < 0 || cur >= bufsize) return;
	range.fit_by_range(IndexRange(0, bufsize - 1)); if (!range) return;
	
	unsigned int il = cur + range.min();
	if (il >= bufsize) il -= bufsize;
	
	unsigned int ir = il + range.count() - 1;
	if (ir >= bufsize) {
		this->former.set(il, bufsize - 1);
		this->latter.set(0, ir - bufsize);
	} else
		this->former.set(il, ir);
}

inline unsigned int CircularBuffer::size() const
{
	return this->bufsize;
}

inline unsigned int CircularBuffer::spike_buffer_size() const
{
	return this->buf_spike_size;
}

inline bool CircularBuffer::is_valid_range(IndexRange range) const
{
	return range && range.max() < this->bufsize;
}

inline unsigned int CircularBuffer::count() const
{
	return this->cnt;
}

inline IndexRange CircularBuffer::range() const
{
	if (this->cnt > 0)
		return IndexRange(0, this->cnt - 1);
	else
		return IndexRange();
}

inline IndexRange CircularBuffer::range_max() const
{
	return IndexRange(0, this->bufsize - 1);
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
	if (i >= cnt_ovr + this->cnt)
		return this->cnt - 1;
	if (i >= cnt_ovr)
		return i - cnt_ovr;
	else
		return 0;
}

inline IndexRange CircularBuffer::range_to_abs(IndexRange range) const
{
	IndexRange range_abs = range;
	range_abs.move(this->cnt_overwrite);
	return range_abs;
}

inline IndexRange CircularBuffer::range_to_rel(IndexRange range_abs) const
{
	IndexRange range = range_abs;
	range.min_move_to(this->index_to_rel(range_abs.min()));
	return range;
}

inline float& CircularBuffer::item(unsigned int i) const
{
	if (i >= this->bufsize)
		throw std::out_of_range("CircularBuffer::item(): index exceeds the buffer size.");
	
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

inline void CircularBuffer::push(float val, bool spike_check, bool lock)
{
	if (! this->buf) return;
	if (lock) this->lock(true);
	
	*this->end = val;
	this->end = this->ptr_inc(this->end);
	
	if (this->cnt < this->bufsize)
		this->cnt++;
	else
		this->cnt_overwrite++;
	
	if (spike_check)
		this->spike_check();
	
	if (lock) this->unlock();
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

inline ValueRange CircularBuffer::get_value_range(unsigned int chk_step)
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
		unsigned char i = 0; //faster than using int
		while (this->flag_lock.test_and_set(std::memory_order_acquire)) {
			sleep_for(microseconds((int) pow(2, i))); if (i < 12) i++;
		}
	}
	
	if (! for_writing)
		++this->read_lock_counter; //read_lock_counter > 0: locked for reading
}

inline void CircularBuffer::unlock()
{
	if (this->read_lock_counter.load(std::memory_order_acquire) > 0) {
		int counter = --this->read_lock_counter;
		if (counter > 0) return;
		if (counter < 0) this->read_lock_counter = 0; //this shouldn't happen if lock/unlock are paired
	}
	this->flag_lock.clear(std::memory_order_release);
}

/*------------------------------ private functions ------------------------------*/

inline float* CircularBuffer::ptr_inc(float* p, unsigned int inc) const
{
	p += inc;
	if (p > this->bufend)
		p -= this->bufsize;
	return p;
}

inline float* CircularBuffer::item_addr(unsigned int i) const
{
	if (! this->is_full())
		return this->ptr_inc(this->buf, i);
	else
		return this->ptr_inc(this->end, i);
}

inline BufRangeMap CircularBuffer::map_from(IndexRange range) const
{
	return BufRangeMap(range, this->bufsize, this->item_addr(0) - this->buf);
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

