// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#ifndef SIMPLE_CAIRO_PLOT_AXIS_RANGE_H
#define SIMPLE_CAIRO_PLOT_AXIS_RANGE_H

#include <cmath>

namespace SimpleCairoPlot
{
class ValueRange; class AxisValues;
class IndexRange;

using Range = ValueRange;
using AxisRange = ValueRange;
using UIntRange = IndexRange;

class ValueRange //closed range
{
public:
	ValueRange(float min, float max);
	
	float min() const;
	float max() const;
	float length() const;
	float center() const;
	
	bool operator==(const ValueRange& range) const;
	bool operator!=(const ValueRange& range) const;
	
	bool contain(float val) const;
	bool contain(ValueRange range) const;
	float fit_value(float val) const;
	ValueRange cut_range(ValueRange range) const;
	ValueRange fit_range(ValueRange range) const;
	
	float map(float val, ValueRange range, bool reverse = false) const;
	float map_reverse(float val, ValueRange range) const;
	
	void set(float min, float max);
	void move(float offset);
	void min_move_to(float min); //max moves with min
	void max_move_to(float max); //min moves with max
	void fit_by_range(ValueRange range);
	void scale(float factor, float cursor);
	void scale(float factor);
	
private:
	float val_min, val_max;
	float val_length;
};

class AxisValues
{
public:
	AxisValues(ValueRange range, unsigned int divider, bool adjust = true);
	unsigned int count() const;
	float operator[](unsigned int i) const;
	
private:
	enum {Cnt_Choices = 5};
	const float Choices[Cnt_Choices] = {1, 2, 2.5, 5, 10};
	
	float val_first, cell_width;
	unsigned int cnt;
};

class IndexRange
{
public:
	IndexRange();
	IndexRange(unsigned long int min, unsigned long int max);
	IndexRange(const ValueRange& ax);
	
	operator ValueRange() const;
	ValueRange to_axis(long int offset = 0) const;
	
	operator bool() const;
	unsigned long int min() const;
	unsigned long int max() const;
	unsigned long int count() const;
	unsigned long int length() const;
	unsigned long int count_by_step(unsigned int step) const;
	
	bool operator==(const IndexRange& range) const;
	bool operator!=(const IndexRange& range) const;
	
	bool contain(unsigned long int i) const;
	bool contain(IndexRange range) const;
	bool intersected_not_left_of(IndexRange range) const;
	unsigned long int fit_index(unsigned long int index) const;
	unsigned long int fit_value(unsigned long int val) const; //same as fit_index()
	IndexRange cut_range(IndexRange range) const;
	IndexRange fit_range(IndexRange range) const;
	
	float map(unsigned long int val, ValueRange range, bool reverse = false) const;
	float map_reverse(unsigned long int val, ValueRange range) const;
	
	void set(unsigned long int min, unsigned long int max);
	void move(long int offset);
	void min_move_to(unsigned long int min); //min moves with max
	void max_move_to(unsigned long int max); //max moves with min
	void fit_by_range(IndexRange range);
	void step_align_with(IndexRange range, unsigned int step);

private:
	bool valid;
	unsigned long int val_min, val_max, cnt;
};

long int subtract(unsigned long int a, unsigned long int b);
IndexRange intersection(IndexRange range1, IndexRange range2);

/*------------------------------ ValueRange functions ------------------------------*/

inline ValueRange::ValueRange(float min, float max)
{
	this->set(min, max);
}

inline float ValueRange::min() const
{
	return this->val_min;
}

inline float ValueRange::max() const
{
	return this->val_max;
}

inline float ValueRange::length() const
{
	return this->val_length;
}

inline float ValueRange::center() const
{
	return (this->val_min + this->val_max) / 2.0;
}

inline bool ValueRange::operator==(const ValueRange& range) const
{
	return this->val_min == range.min() && this->val_max == range.max();
}

inline bool ValueRange::operator!=(const ValueRange& range) const
{
	return this->val_min != range.min() || this->val_max != range.max();
}

inline bool ValueRange::contain(float val) const
{
	return this->val_min <= val && val <= this->val_max;
}

inline bool ValueRange::contain(ValueRange range) const
{
	return this->val_min <= range.min() && range.max() <= this->val_max;
}

inline float ValueRange::fit_value(float val) const
{
	if (val < this->val_min) return this->val_min;
	if (val > this->val_max) return this->val_max;
	return val;
}

inline ValueRange ValueRange::cut_range(ValueRange range) const
{
	return ValueRange(this->fit_value(range.min()), this->fit_value(range.max()));
}

inline ValueRange ValueRange::fit_range(ValueRange range) const
{
	if (this->contain(range)) return range;
	if (range.length() >= this->val_length) return *this;
	
	ValueRange range_new = range;
	if (range.min() < this->val_min)
		range_new.min_move_to(this->val_min);
	else if (range.max() > this->val_max)
		range_new.max_move_to(this->val_max);
	
	return range_new;
}

inline float ValueRange::map(float val, ValueRange range, bool reverse) const
{
	if (this->val_length == 0) return 0;
	
	val = this->fit_value(val); //ensures that val is valid
	val = (range.length() * (val - this->val_min)) / this->val_length;
	if (reverse) val = range.length() - val;
	val += range.min(); return val;
}

inline float ValueRange::map_reverse(float val, ValueRange range) const
{
	return this->map(val, range, true);
}

inline void ValueRange::set(float min, float max)
{
	if (min <= max) {
		this->val_min = min; this->val_max = max;
	} else {
		this->val_min = max; this->val_max = min;
	}
	
	this->val_length = this->val_max - this->val_min;
}

inline void ValueRange::move(float offset)
{
	this->val_min += offset; this->val_max += offset;
}

inline void ValueRange::min_move_to(float min)
{
	this->move(min - this->val_min);
}

inline void ValueRange::max_move_to(float max)
{
	this->move(max - this->val_max);
}

inline void ValueRange::fit_by_range(ValueRange range)
{
	*this = range.fit_range(*this);
}

inline void ValueRange::scale(float factor, float cursor)
{
	if (factor < 0) factor = -factor;
	
	float l = cursor - this->val_min, r = this->val_max - cursor;
	l *= factor; r *= factor;
	this->set(cursor - l, cursor + r);
}

inline void ValueRange::scale(float factor)
{
	this->scale(factor, this->center());
}

/*------------------------------ AxisValues functions ------------------------------*/

inline AxisValues::AxisValues(ValueRange range, unsigned int divider, bool adjust)
{
	using namespace std;
	if (divider == 0) divider = 1;
	
	if (adjust && range.length() > 0) {
		// inspired by: <https://blog.csdn.net/tiangej/article/details/47731501>
		float cell_width_raw = range.length() / divider;
		int exponent = floor(log(cell_width_raw) / log(10.0));
		float power = pow(10, exponent), coefficient = cell_width_raw / power;
		
		unsigned int i;
		for (i = 0; i < Cnt_Choices - 1; i++)
			if (coefficient < Choices[i + 1]) {
				float diff_left = coefficient - Choices[i],
				      diff_right = Choices[i + 1] - coefficient;
				if (diff_left > diff_right) i++;
				break;
			}
		
		this->cell_width = Choices[i] * power;
		this->val_first = ceil(range.min() / this->cell_width) * this->cell_width;
		this->cnt = (range.max() - this->val_first + this->cell_width / 200.0) / this->cell_width + 1;
	}
	else {
		this->val_first = range.min();
		this->cell_width = range.length() / divider;
		this->cnt = divider + 1;
	}
}

inline unsigned int AxisValues::count() const
{
	return this->cnt;
}

inline float AxisValues::operator[](unsigned int i) const
{
	return this->val_first + i*this->cell_width;
}

/*------------------------------ IndexRange functions ------------------------------*/

inline long int subtract(unsigned long int a, unsigned long int b)
{
	if (a >= b)
		return a - b;
	else
		return -(long int)(b - a);
}

inline IndexRange intersection(IndexRange range1, IndexRange range2)
{
	return range1.cut_range(range2);
}

inline IndexRange::IndexRange()
{
	this->valid = false;
	this->cnt = this->val_min = this->val_max = 0;
}

inline IndexRange::IndexRange(unsigned long int min, unsigned long int max)
{
	this->set(min, max);
}

inline IndexRange::IndexRange(const ValueRange& ax)
{
	if (ax.max() < 0) {
		this->cnt = this->val_min = this->val_max = 0;
		this->valid = false; return;
	}
	
	using namespace std;
	float min = ax.min(); if (min < 0) min = 0;
	this->set(round(min), round(ax.max()));
}

inline IndexRange::operator ValueRange() const
{
	return this->to_axis();
}

inline ValueRange IndexRange::to_axis(long int offset) const
{
	if (! this->valid) return ValueRange(-1, -1);
	return ValueRange((float)this->val_min + offset,
	                  (float)this->val_max + offset);
}

inline IndexRange::operator bool() const
{
	return this->valid;
}

inline unsigned long int IndexRange::min() const
{
	return this->val_min;
}

inline unsigned long int IndexRange::max() const
{
	return this->val_max;
}

inline unsigned long int IndexRange::count() const
{
	return this->cnt;
}

inline unsigned long int IndexRange::length() const
{
	if (! this->valid) return 0;
	return this->count() - 1;
}

inline unsigned long int IndexRange::count_by_step(unsigned int step) const
{
	if (!this->valid || step == 0) return 0;
	unsigned int quo = this->count() / step, rem = this->count() % step;
	if (rem > 0) quo++; return quo;
}

inline bool IndexRange::operator==(const IndexRange& range) const
{
	if (!this->valid && !range.valid) return true;
	return this->valid && range.valid
	    && this->val_min == range.min() && this->val_max == range.max();
}

inline bool IndexRange::operator!=(const IndexRange& range) const
{
	return !(*this == range);
}

inline bool IndexRange::contain(unsigned long int i) const
{
	return this->valid && this->val_min <= i && i <= this->val_max;
}

inline bool IndexRange::contain(IndexRange range) const
{
	return this->valid && range.valid
	    && this->val_min <= range.val_min && range.val_max <= this->val_max;
}

inline bool IndexRange::intersected_not_left_of(IndexRange range) const
{
	if (!this->valid && !range.valid) return false;
	return range.contain(this->val_min) && this->contain(range.max());
}

inline unsigned long int IndexRange::fit_index(unsigned long int index) const
{
	if (! this->valid) return 0;
	if (index < this->val_min) return this->val_min;
	if (index > this->val_max) return this->val_max;
	return index;
}

inline unsigned long int IndexRange::fit_value(unsigned long int val) const
{
	return this->fit_index(val);
}

inline IndexRange IndexRange::cut_range(IndexRange range) const
{
	if (!this->valid || !range.valid) return IndexRange();
	if (!this->contain(range.min()) && !this->contain(range.max()) && !range.contain(*this))
		return IndexRange();
	return IndexRange(this->fit_index(range.min()), this->fit_index(range.max()));
}

inline IndexRange IndexRange::fit_range(IndexRange range) const
{
	if (!this->valid || !range.valid) return IndexRange();
	if (this->contain(range)) return range;
	if (range.count() >= this->cnt) return *this;
	
	IndexRange range_new = range;
	if (range.min() < this->val_min)
		range_new.min_move_to(this->val_min);
	else if (range.max() > this->val_max)
		range_new.max_move_to(this->val_max);
	
	return range_new;
}

inline float IndexRange::map(unsigned long int val, ValueRange range, bool reverse) const
{
	val = this->fit_value(val);
	return ValueRange(0, this->length()).map(val - this->val_min, range, reverse);
}

inline float IndexRange::map_reverse(unsigned long int val, ValueRange range) const
{
	return this->map(val, range, true);
}

inline void IndexRange::set(unsigned long int min, unsigned long int max)
{
	if (max < min) {
		this->cnt = this->val_min = this->val_max = 0;
		this->valid = false; return;
	}
	
	this->val_min = min; this->val_max = max;
	this->cnt = max - min + 1;
	this->valid = true;
}

inline void IndexRange::move(long int offset)
{
	if (! this->valid) return;
	if (offset < -(long long int)this->val_min)
		offset = -(long long int)this->val_min;
	this->val_min += offset; this->val_max += offset; //unsigned long int + long int works
}

inline void IndexRange::min_move_to(unsigned long int min)
{
	// note: signed - unsigned is dangerous
	if (! this->valid) return;
	this->move(subtract(min, this->val_min));
}

inline void IndexRange::max_move_to(unsigned long int max)
{
	if (! this->valid) return;
	this->move(subtract(max, this->val_max));
}

inline void IndexRange::fit_by_range(IndexRange range)
{
	*this = range.fit_range(*this);
}

inline void IndexRange::step_align_with(IndexRange range, unsigned int step)
{
	if (! range) return;
	
	// note: signed *,/,<,> unsigned is dangerous
	long int diff_min = subtract(this->min(), range.min());
	diff_min = round(diff_min / (long int)step) * (long int)step;
	
	unsigned long int new_min, new_max;
	
	if (diff_min >= 0 || -diff_min < (long long int)range.min())
		new_min = range.min() + diff_min;
	else
		new_min = 0;
	
	new_max = new_min + this->count_by_step(step)*step;
	while (new_max > this->max()) {
		if (new_max >= step)
			new_max -= step;
		else
			new_max = 0;
	}
	
	this->set(new_min, new_max);
}

}
#endif

