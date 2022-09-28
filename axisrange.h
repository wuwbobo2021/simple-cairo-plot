// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#ifndef SIMPLE_CAIRO_PLOT_AXIS_RANGE_H
#define SIMPLE_CAIRO_PLOT_AXIS_RANGE_H

#include <cmath>

namespace SimpleCairoPlot
{

class AxisRange //closed range
{
	float val_min, val_max;
	float val_length;

public:
	AxisRange(float min, float max);
	
	float min() const;
	float max() const;
	float length() const;
	float center() const;
	
	bool operator==(const AxisRange& range) const;
	bool operator!=(const AxisRange& range) const;
	
	bool contain(float val) const;
	bool contain(AxisRange range) const;
	bool intersected_not_left_of(AxisRange range) const;
	float fit_value(float val) const;
	AxisRange cut_range(AxisRange range) const;
	AxisRange fit_range(AxisRange range) const;
	
	float map(float val, float target_width, bool reverse = false) const;
	float map(float val, AxisRange range, bool reverse = false) const;
	float map_reverse(float val, float target_width) const;
	float map_reverse(float val, AxisRange range) const;
	
	void set(float min, float max);
	
	void move(float offset);
	void min_move_to(float min); //max moves with min
	void max_move_to(float max); //min moves with max
	
	void fit_by_range(AxisRange range);
	
	void scale(float factor, float cursor);
	void scale(float factor);
	
	void set_int();
};

using Range = AxisRange;

inline AxisRange::AxisRange(float min, float max)
{
	this->set(min, max);
}

inline float AxisRange::min() const
{
	return this->val_min;
}

inline float AxisRange::max() const
{
	return this->val_max;
}

inline float AxisRange::length() const
{
	return this->val_length;
}

inline float AxisRange::center() const
{
	return (this->val_min + this->val_max) / 2.0;
}

inline bool AxisRange::operator==(const AxisRange& range) const
{
	return this->val_min == range.min() && this->val_max == range.max();
}

inline bool AxisRange::operator!=(const AxisRange& range) const
{
	return this->val_min != range.min() || this->val_max != range.max();
}

inline bool AxisRange::contain(float val) const
{
	return this->val_min <= val && val <= this->val_max;
}

inline bool AxisRange::contain(AxisRange range) const
{
	return this->val_min <= range.min() && range.max() <= this->val_max;
}

inline bool AxisRange::intersected_not_left_of(AxisRange range) const
{
	return range.contain(this->val_min) && this->contain(range.max());
}

inline float AxisRange::fit_value(float val) const
{
	if (val < this->val_min) return this->val_min;
	if (val > this->val_max) return this->val_max;
	return val;
}

inline AxisRange AxisRange::cut_range(AxisRange range) const
{
	return AxisRange(this->fit_value(range.min()), this->fit_value(range.max()));
}

inline AxisRange AxisRange::fit_range(AxisRange range) const
{
	if (this->contain(range)) return range;
	if (range.length() >= this->val_length) return *this;
	
	AxisRange range_new = range;
	if (range.min() < this->val_min)
		range_new.min_move_to(this->val_min);
	else if (range.max() > this->val_max)
		range_new.max_move_to(this->val_max);
	
	return range_new;
}

inline float AxisRange::map(float val, float target_width, bool reverse) const
{
	if (this->val_length == 0) return 0;
	
	val = fit_value(val); //ensures that val is valid
	val = (target_width * (val - this->val_min)) / this->val_length;
	if (reverse) val = target_width - val;
	return val;
}

inline float AxisRange::map(float val, AxisRange range, bool reverse) const
{
	return this->map(val, range.length(), reverse) + range.min();
}

inline float AxisRange::map_reverse(float val, float target_width) const
{
	return this->map(val, target_width, true);
}

inline float AxisRange::map_reverse(float val, AxisRange range) const
{
	return this->map(val, range, true);
}

inline void AxisRange::set(float min, float max)
{
	if (min <= max) {
		this->val_min = min; this->val_max = max;
	} else {
		this->val_min = max; this->val_max = min;
	}
	
	this->val_length = this->val_max - this->val_min;
}

inline void AxisRange::move(float offset)
{
	this->val_min += offset; this->val_max += offset;
}

inline void AxisRange::min_move_to(float min)
{
	float offset = min - this->val_min;
	this->val_min = min;
	this->val_max += offset;
}

inline void AxisRange::max_move_to(float max)
{
	float offset = max - this->val_max;
	this->val_max = max;
	this->val_min += offset;
}

inline void AxisRange::fit_by_range(AxisRange range)
{
	AxisRange range_new = range.fit_range(*this);
	this->set(range_new.min(), range_new.max());
}

inline void AxisRange::scale(float factor, float cursor)
{
	if (factor < 0) factor = -factor;
	
	float l = cursor - this->val_min, r = this->val_max - cursor;
	l *= factor; r *= factor;
	this->set(cursor - l, cursor + r);
}

inline void AxisRange::scale(float factor)
{
	this->scale(factor, this->center());
}

inline void AxisRange::set_int()
{
	using namespace std;
	this->val_min = round(this->val_min);
	this->val_max = round(this->val_max);
}

class AxisValues
{
	enum {Cnt_Choices = 5};
	const float Choices[Cnt_Choices] = {1, 2, 2.5, 5, 10};
	
	float val_first, cell_width;
	unsigned int cnt;

public:
	AxisValues(AxisRange range, unsigned int divider, bool adjust = true);
	unsigned int count() const;
	float operator[](unsigned int i) const;
};

inline AxisValues::AxisValues(AxisRange range, unsigned int divider, bool adjust)
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

}
#endif

