// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_AXIS_RANGE_H
#define SIMPLE_CAIRO_PLOT_AXIS_RANGE_H

#include <cmath>
#include <stdexcept>

namespace SimpleCairoPlot
{

// Closed range
class AxisRange
{
	float val_min, val_max;
	float val_length;

public:
	AxisRange(float min, float max);
	
	float min() const;
	float max() const;
	float length() const;
	bool contain(float val) const;
	bool contain(AxisRange range) const;
	float cut_value(float val) const;
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

inline AxisRange::AxisRange(float min, float max):
	val_min(min), val_max(max),
	val_length(max - min)
{
	if (min > max)
		throw std::invalid_argument("AxisRange::AxisRange(): min > max.");
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

inline bool AxisRange::contain(float val) const
{
	return this->val_min <= val && val <= this->val_max;
}

inline bool AxisRange::contain(AxisRange range) const
{
	return this->val_min <= range.min() && range.max() <= this->val_max;
}

inline float AxisRange::cut_value(float val) const
{
	if (val < this->val_min) return this->val_min;
	if (val > this->val_max) return this->val_max;
	return val;
}

inline AxisRange AxisRange::cut_range(AxisRange range) const
{
	return AxisRange(this->cut_value(range.min()), this->cut_value(range.max()));
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
	
	val = cut_value(val); //ensures that val is valid
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
	if (min > max)
		throw std::invalid_argument("AxisRange::set(): min >= max.");
	this->val_min = min; this->val_max = max;
	this->val_length = max - min;
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
	if (factor <= 0)
		throw std::invalid_argument("AxisRange::scale(): minus factor.");
	
	float l = cursor - this->val_min, r = this->val_max - cursor;
	l *= factor; r *= factor;
	this->set(cursor - l, cursor + r);
}

inline void AxisRange::scale(float factor)
{
	this->scale(factor, (this->val_min + this->val_max) / 2.0);
}

inline void AxisRange::set_int()
{
	this->val_min = round(this->val_min);
	this->val_max = round(this->val_max);
}

}
#endif

