// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_PLOTTING_AREA_H
#define SIMPLE_CAIRO_PLOT_PLOTTING_AREA_H

#include <sstream>
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
	CircularBuffer* source = NULL; //data source
	
	// range of indexes in the buffer and y-axis values that are covered by the area
	AxisRange range_x = AxisRange(0, 100), range_y = AxisRange(0, 10);
	unsigned int index_step = 1; //it will be adjusted according to the width of the area when range_x is too wide
	
	float axis_x_unit = 1; //it should be the data interval, index values are multiplied by the unit
	unsigned int axis_x_divider = 5, axis_y_divider = 6; //how many segments the axis should be divided into by the grid
	
	float axis_y_length_min = 0; //minimum range length of y-axis range in auto-set mode
	
	Gdk::RGBA color_grid, color_text; //auto set in PlottingArea::on_style_updated()
	
	// used for auto-refresh mode
	std::thread* thread_timer;
	Glib::Dispatcher dispatcher; //used for thread safety
	bool flag_auto_refresh = false;
	unsigned int refresh_interval = 40; //25 Hz
	
	std::stringstream sst; //used for printing value labels for the grid
	
	// used for the controlling the interval of range y auto setting
	unsigned int counter1 = 0, counter2 = 0;
	bool flag_check_range_y = false, flag_adapt = false;
	
	void refresh_loop();
	void refresh_callback();
	
	// inherits Gtk::Widget, implements drawing procedures
	void on_style_updated() override;
	bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
	
	void adjust_index_step();
	Gtk::Allocation draw_grid(const Cairo::RefPtr<Cairo::Context>& cr);
	void plot(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc);
	
public:
	enum {Border_X_Left = 40, Border_Y = 10};
	
	std::string axis_x_unit_name = "", axis_y_unit_name = ""; //both names should be short, especially axis_y_unit_name
	Gdk::RGBA color_plot;
	
	bool option_show_axis_x_values = true;
	bool option_show_axis_y_values = true;
	bool option_anti_alias = false;
	
	bool option_auto_goto_end = true; //whether or not range_x should goto the end of buffer automatically
	bool option_auto_set_range_y = true; //whether or not range_y should be determined automatically
	bool option_auto_set_zero_bottom = true; //if the bottom of range y should be zero in auto-set mode
	
	PlottingArea(); void init(CircularBuffer* buf);
	PlottingArea(CircularBuffer* buf);
	virtual ~PlottingArea();

	bool set_refresh_mode(bool auto_refresh = true, unsigned int interval = 0); //in milliseconds
	void refresh(bool forced_check_range_y = false, bool forced_adapt = false); //refresh manually, also thread-safe
	
	AxisRange get_range_x() const;
	AxisRange get_range_y() const;
	
	bool set_axis_divider(unsigned int x_div, unsigned int y_div);
	bool set_axis_x_unit(float unit);
	bool set_axis_y_range_length_min(float length_min);
	
	// functions only for manual refresh mode except set_range_x()
	// (the only way to change the index range width even in automatic mode).
	bool set_range_x(AxisRange range);
	bool set_range_y(AxisRange range);
	void range_x_goto_end();
	void range_y_auto_set(bool adapt = true);
};

}
#endif

