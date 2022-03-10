// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_LOGGER_H
#define SIMPLE_CAIRO_PLOT_LOGGER_H

#include <vector>

#include <gtkmm/box.h>
#include <gtkmm/scrollbar.h>

#include "plottingarea.h"

namespace SimpleCairoPlot
{

typedef DataAccessFunc float(*)(void*);

// used for the logger to access current values of logging variables
struct DataAccessPtr
{
	bool is_func_ptr = false;
	
	// used if it's a data pointer
	float* addr_data;
	
	// used if it's a function pointer
	DataAccessFunc addr_func;
	void* addr_obj;
	
	Gdk::Color color_plot;
	
	DataAccessPtr();
	float read() const;
}

inline DataAccessPtr::DataAccessPtr()
{
	this->color_plot.set_rgb(1.0, 0.0, 0.0); //red
}

inline DataAccessPtr::read() const
{
	if (this->is_func_ptr)
		return this->addr_func(this->addr_obj);
	else
		return *(this->addr_data);
}

class Logger: Gtk::Box
{
	DataAccessPtr* ptrs;
	CircularBuffer* bufs;
	PlottingArea* areas;
	Gtk::ScrollBar scroll_bar;
	
	unsigned int interval = 100;
	
public:
	const unsigned int items_count;
	
	Logger(vector<DataAccessPtr>& ptrs, unsigned int buf_size);
	virtual ~Logger();
	
	bool set_interval(unsigned int new_interval); //interval of reading current values, in milliseconds
	
	bool set_index_range(unsigned int range_width); //range_width + 1 is the amount of data shows in each area
	bool set_index_unit(float unit); //it should be the data interval, index values are multiplied by the unit
	bool set_axis_divider(unsigned int x_div, unsigned int y_div); //how many segments the axis should be divided into
	
	void set_option_auto_set_range_y(bool set); //if it's not set, the user must set the range for each area
	void set_option_show_axis_x_values(bool set); //only that of the bottommost is shown
	void set_option_show_axis_y_values(bool set); //shown in left border of each area
	void set_option_anti_alias(bool set);
	
	void start(); //start logging
	void stop(bool clear_buf = false);
	
	CircularBuffer& buffer(unsigned int index); //direct access to each data buffer
	PlottingArea& area(unsigned int index); //direct access to each plotting area
}

}

#endif

