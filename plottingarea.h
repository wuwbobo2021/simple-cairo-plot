// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#ifndef SIMPLE_CAIRO_PLOT_PLOTTING_AREA_H
#define SIMPLE_CAIRO_PLOT_PLOTTING_AREA_H

#include <sstream>
#include <thread>

#include <gdkmm/color.h>
#include <cairo.h>
#include <cairomm/context.h>
#include <glibmm/dispatcher.h>
#include <gtkmm/drawingarea.h>

#include <simple-cairo-plot/circularbuffer.h>

namespace SimpleCairoPlot
{

const unsigned int Plot_Data_Amount_Limit_Min = 512;

class PlottingArea: public Gtk::DrawingArea
{
	CircularBuffer* source = NULL; //data source
	unsigned long int* buf_spike = NULL;
	
	// used for buffering the cairo path data
	Range plot_data_amount_max_range = AxisRange(Plot_Data_Amount_Limit_Min, 2048);
	cairo_path_data_t* buf_cr = NULL; int buf_cr_size, i_buf_cr = 1;
	cairo_path_data_t* buf_cr_spike = NULL; int buf_cr_spike_size, i_buf_cr_spike = 1;
	
	// range of indexes in the buffer and y-axis values that are covered by the area
	AxisRange range_x = AxisRange(0, 100), range_y = AxisRange(0, 10);
	unsigned int index_step = 1; //it will be adjusted according to the width of the area when range_x is too wide
	
	float axis_x_unit = 1; //it should be the data interval, index values are multiplied by the unit
	unsigned int axis_x_divider = 5, axis_y_divider = 6; //how many segments the axis should be divided into by the grid
	float axis_y_length_min = 0; //minimum range length of y-axis range in auto-set mode
	
	std::ostringstream oss; //used for printing value labels for the grid
	Gdk::RGBA color_grid, color_text; //auto set in PlottingArea::on_style_updated()
	const std::vector<double> dash_pattern = {10, 2, 2, 2}; //used for drawing average line
	
	Glib::Dispatcher dispatcher; //used for accepting refresh request from another thread
	
	// used for the controlling the interval of range y auto setting
	unsigned int counter1 = 0, counter2 = 0;
	volatile bool flag_check_range_y = false, flag_adapt = false;
	
	// used for auto-refresh mode
	std::thread* thread_timer; 
	bool flag_auto_refresh = false;
	unsigned int refresh_interval = 20; //50 Hz
	
	void refresh_loop();
	
	// override event handlers of Gtk::Widget
	void on_style_updated() override;
	bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
	
	void adjust_index_step();
	Gtk::Allocation draw_grid(const Cairo::RefPtr<Cairo::Context>& cr);
	void plot(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc);
	void draw_average_line(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc);
	
	void buf_cr_add(float x, float y, bool spike = false);
	void buf_cr_clear();
	
public:
	enum {Border_X_Left = 50, Border_Y = 12};
	
	bool option_show_axis_x_values = true; bool option_axis_x_int_values = false;
	bool option_show_axis_y_values = true;
	std::string axis_x_unit_name = "", axis_y_unit_name = ""; //both names should be short, especially axis_y_unit_name
	
	Gdk::RGBA color_plot; bool option_anti_alias = false;
	bool option_fixed_scale = true; //note: tick values of fixed scale can have many decimal digits
	
	bool option_auto_set_range_y = true; //determine range_y automatically
	bool option_auto_set_zero_bottom = true; //if the bottom of range y should be zero in auto-set mode
	
	volatile bool option_auto_goto_end = true; //set range_x to the end of buffer automatically
	volatile bool option_auto_extend_range_x = false; //extend range_x to show all data when option_auto_goto_end is on
	
	bool option_show_average_line = false; //this requires extra calculation, though it was optimized
	
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
	void range_x_extend(bool remain_space = true);
	void range_y_auto_set(bool adapt = true);
};

inline AxisRange PlottingArea::get_range_x() const
{
	return this->range_x;
}

inline AxisRange PlottingArea::get_range_y() const
{
	return this->range_y;
}

// private

inline void PlottingArea::buf_cr_add(float x, float y, bool spike)
{
	if (! spike) {
		buf_cr[i_buf_cr].point.x = x;
		buf_cr[i_buf_cr].point.y = y;
		i_buf_cr += 2;
	} else {
		buf_cr_spike[i_buf_cr_spike].point.x = x;
		buf_cr_spike[i_buf_cr_spike].point.y = y;
		i_buf_cr_spike += 2;
	}
}

inline void PlottingArea::buf_cr_clear()
{
	i_buf_cr = i_buf_cr_spike = 1;
}

}
#endif

