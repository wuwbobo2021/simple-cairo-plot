// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_PLOTTING_AREA_H
#define SIMPLE_CAIRO_PLOT_PLOTTING_AREA_H

#include <thread>

#include <gdkmm/color.h>
#include <cairomm/context.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/drawingarea.h>

#include "circularbuffer.h"

namespace SimpleCairoPlot
{
class PlottingArea: public Gtk::DrawingArea
{
	CircularBuffer& source; //data source
	
	// range of indexes in the buffer and y-axis values that are covered by the area
	AxisRange range_x = AxisRange(0, 100), range_y = AxisRange(0, 10);
	
	// specifies how many segments the axis should be divided into by the grid
	unsigned int axis_x_divider = 5, axis_y_divider = 5;
	float axis_x_unit = 1; //it should be the data interval, index values are multiplied by the unit
	
	// used for auto-refresh mode
	bool option_auto_refresh = true;
	unsigned int refresh_interval = 40; //25 Hz
	
	std::thread* thread_timer;
	Glib::Dispatcher dispatcher; //used for thread safety
	bool flag_stop_auto_refresh = false;
	
	void refresh_loop();
	void refresh_callback();
	
	// inherits Gtk::Widget, implements drawing procedures
	bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
	Gtk::Allocation draw_grid(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc);
	void plot(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc);
	
public:
	bool option_auto_goto_end = true; //whether or not range_x should goto the end of buffer automatically
	bool option_auto_set_range_y = true; //whether or not range_y should be determined automatically
	
	bool option_show_axis_x_values = false;
	bool option_show_axis_y_values = false;
	bool option_anti_alias = false;
	Gdk::Color color_plot;
	
	PlottingArea(CircularBuffer& buf);
	virtual ~PlottingArea();
	
	// functions only for manual refresh mode except set_range_x().
	// (which is the only way to change the index range even in automatic mode)
	bool set_range_x(AxisRange range);
	bool set_range_y(AxisRange range);
	void range_x_goto_end();
	void range_y_auto_set(bool tight = true);
	
	bool set_axis_divider(unsigned int x_div, unsigned int y_div);
	bool set_axis_x_unit(float unit);
	void set_refresh_mode(bool auto_refresh = true, unsigned int interval = 0); //in milliseconds
	void refresh(); //refresh manually, it's also thread-safe
};

}
#endif

