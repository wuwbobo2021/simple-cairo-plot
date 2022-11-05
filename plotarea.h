// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#ifndef SIMPLE_CAIRO_PLOT_AREA_H
#define SIMPLE_CAIRO_PLOT_AREA_H

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
class PlotArea;
class PlotParam; class PlotBuffer; //owned by PlotArea

using PlottingArea = PlotArea; //v1.0.x name

// please skip to class PlotArea

struct PlotParam
{
	// current conditions
	unsigned int data_cnt = 0; unsigned long int data_cnt_overall = 0;
	Gtk::Allocation alloc, alloc_outer; //topleft point of alloc_outer is always (0, 0)
	unsigned int y_av_alloc = 0; //don't care if option_show_average_line is not set
	
	IndexRange range_x; //different from PlotArea::range_x, it's the "absolute" index range of plotting data
	ValueRange range_y = ValueRange(0, 10);
	unsigned int index_step = 1; //it will be adjusted when range_x is too wide
	
	// stored options
	Gdk::RGBA color_plot; bool option_anti_alias = false;
	
	unsigned int axis_x_divider = 5, axis_y_divider = 6;
	bool option_fixed_scale = true;
	bool option_show_axis_x_values = true; bool option_axis_x_int_values = false;
	bool option_show_axis_y_values = true;
	bool option_show_average_line = false;
	float axis_x_unit = 1;
	std::string axis_x_unit_name = "", axis_y_unit_name = "";
	
	operator bool() const;
	bool operator==(const PlotParam& prev) = delete;
	bool operator!=(const PlotParam& prev) = delete;
	bool reuse_graph(const PlotParam& prev) const;
	bool reuse_data(const PlotParam& prev) const;
	
	IndexRange data_range_x() const; //the part of range_x currently available
	
	float alloc_x_step() const;
	AxisRange alloc_x() const;
	AxisRange alloc_y() const;
};

class PlotBuffer
{
public:
	PlotBuffer(); void init(CircularBuffer* src, unsigned int cnt_limit = 0);
	PlotBuffer(CircularBuffer* src, unsigned int cnt_limit = 0);
	~PlotBuffer();
	
	const PlotParam& get_param() const;
	bool sync(const PlotParam& param, bool forced_sync = false);
	void cairo_load(const Cairo::RefPtr<Cairo::Context>& cr, bool forced_redraw = false);
	
private:
	CircularBuffer* source;
	
	unsigned long int* buf_spike = NULL;
	
	unsigned int buf_cr_cnt_max;
	cairo_path_data_t* buf_cr = NULL; unsigned int buf_cr_size, i_buf_cr = 1;
	cairo_path_data_t* buf_cr_spike = NULL; unsigned int buf_cr_spike_size, i_buf_cr_spike = 1;
	
	IndexRange range_data; //loaded data range in the buffer (absolute index)
	unsigned int cur_buf_cr = 0, cnt_buf_cr = 0;
	unsigned int cur_ext = 0, cnt_ext = 0; //don't care if flag_redraw or forced_redraw is set
	
	PlotParam param;
	bool flag_redraw = true; //set by sync(), cleared by cairo_stroke()
	float buf_cr_x_step = 0; //set by buf_cr_refresh_x()
	
	void buf_cr_refresh_x(float x_step); //set all point x values (need to be translated) in the buffer
	void buf_cr_load(unsigned int cur_buf_cr, IndexRange range_data);
	void buf_cr_spike_sync();
	void buf_cr_add(float y); //it expects i_buf_cr to be an odd index (see cairo_path_data_t reference)
	void buf_cr_spike_add(float x, float y);
	
	unsigned int cur_move(unsigned int cur, int offset) const;
	unsigned int i_to_cur(unsigned int i) const;
	unsigned int cur_to_i(unsigned int cur) const; //returns an odd index
};

class PlotArea: public Gtk::DrawingArea
{
public:
	enum {Plot_Data_Amount_Limit_Min = 512};
	enum {Border_X_Left = 50, Border_Y = 14};
	
	PlotArea(); void init(CircularBuffer* buf);
	PlotArea(CircularBuffer* buf);
	virtual ~PlotArea();
	
	// functions below can be called in another thread
	
	IndexRange get_range_x() const; //current x-axis source data range
	ValueRange get_range_y() const; //current y-axis value range
	
	// forced_check_range_y and forced_adapt make sense when option_auto_set_range_y is set
	void refresh(bool forced_check_range_y = false, bool forced_adapt = false, bool forced_sync = false);
	bool set_refresh_mode(bool auto_refresh = true, unsigned int interval = 0); //in milliseconds
	
	// range control
	bool set_range_x(IndexRange range); //the only way to change index range width
	bool set_range_y_length_min(float length_min); //minimum range length of y-axis range in auto-set mode
	void set_option_auto_goto_end(bool set); //set range_x to the end of buffer automatically, default: true
	void set_option_auto_extend_range_x(bool set); //extend to show all data in goto-end mode (lower CPU usage), default: false
	void set_option_auto_set_range_y(bool set); //set range_y automatically, default: true
	void set_option_auto_set_zero_bottom(bool set); //always set bottom of range y to 0 in auto-set mode, default: true
	void range_x_goto_end(); void range_x_extend(bool remain_space = true);
	bool set_range_y(ValueRange range); void range_y_auto_set(bool adapt = true);
	
	// grid options
	bool set_axis_divider(unsigned int x_div, unsigned int y_div); //how many segments the axis should be divided into
	bool set_axis_x_unit(float unit); //it should be the data interval, index values are multiplied by the unit
	void set_axis_x_unit_name(std::string str_unit); //short name is expected; it's shown when option_show_axis_x_values is set
	void set_axis_y_unit_name(std::string str_unit); //it should be as short as possible; when option_show_axis_y_values is set
	void set_option_fixed_scale(bool set); //default: true. note: tick values of fixed scale can have many decimal digits
	void set_option_show_axis_x_values(bool set); //show tick values at the bottom, default: true
	void set_option_axis_x_int_values(bool set); //remove decimal digits in x-axis tick values, default: false
	void set_option_show_axis_y_values(bool set); //show tick values left of y-axis, default: true
	void set_option_show_average_line(bool set); //this requires extra calculation (optimized), default: false
	
	// plotting style options
	void set_plot_color(Gdk::RGBA color);
	void set_option_anti_alias(bool set);
	
private:
	CircularBuffer* source = NULL; //data source
	PlotBuffer buf_plot; // used for buffering the cairo path data
	
	UIntRange plot_data_amount_max_range = UIntRange(Plot_Data_Amount_Limit_Min, 2048); //adjust range
	
	IndexRange range_x = IndexRange(0, 100);
	float range_y_length_min = 0;
	PlotParam param;
	
	volatile bool option_auto_goto_end = true;
	volatile bool option_auto_extend_range_x = false;
	
	bool option_auto_set_range_y = true;
	bool option_auto_set_zero_bottom = true;
	
	// used for controlling the interval of range y auto setting
	unsigned int counter1 = 0, counter2 = 0;
	volatile bool flag_check_range_y = false, flag_adapt = false, flag_sync = false;
	
	std::ostringstream oss; //used for printing value labels for the grid
	const std::vector<double> dash_pattern = {10, 2, 2, 2}; //used for drawing average line
	bool flag_set_colors = true; //set background/grid/text colors on first signal_size_allocation
	Gdk::RGBA color_back, color_grid, color_text;
	
	Glib::Dispatcher dispatcher; //used for accepting refresh request from another thread
	volatile bool flag_drawing = false;
	// used for auto-refresh mode
	std::thread* thread_timer;
	volatile bool flag_auto_refresh = false;
	unsigned int refresh_interval = 40; //25 Hz
	
	void on_style_updated() override;
	void on_size_allocation(Gtk::Allocation& allocation);
	void adjust_index_step();
	bool on_draw(const Cairo::RefPtr<Cairo::Context>& cr) override;
	
	void refresh_loop(); //for auto-refresh mode
	
	void draw(Cairo::RefPtr<Cairo::Context> cr = (Cairo::RefPtr<Cairo::Context>)nullptr);
	void draw_grid(Cairo::RefPtr<Cairo::Context> cr, const PlotParam& param, bool not_erase = true);
};

inline IndexRange PlotArea::get_range_x() const
{
	return this->range_x;
}

inline ValueRange PlotArea::get_range_y() const
{
	return this->param.range_y;
}

/*------------------------------ PlotParam, PlotBuffer functions ------------------------------*/

inline PlotParam::operator bool() const
{
	return data_cnt > 1
	    && index_step > 0
	    && range_x.count_by_step(index_step) > 1
	    && range_y.length() > 0
	    && !alloc.has_zero_area();
}

inline IndexRange PlotParam::data_range_x() const
{
	if (this->data_cnt_overall == 0 || this->data_cnt == 0) return IndexRange();
	IndexRange available(0, this->data_cnt - 1);
	available.max_move_to(this->data_cnt_overall - 1);
	return intersection(this->range_x, available);
}

inline float PlotParam::alloc_x_step() const
{
	if (this->range_x.length() == 0) return 0;
	return ((float)this->alloc.get_width() / this->range_x.length()) * this->index_step;
}

inline AxisRange PlotParam::alloc_x() const
{
	return AxisRange(this->alloc.get_x(), this->alloc.get_x() + this->alloc.get_width());
}

inline AxisRange PlotParam::alloc_y() const
{
	return AxisRange(this->alloc.get_y(), this->alloc.get_y() + this->alloc.get_height());
}

inline const PlotParam& PlotBuffer::get_param() const
{
	return this->param;
}

inline void PlotBuffer::buf_cr_add(float y)
{
	this->buf_cr[this->i_buf_cr].point.y = y;
	this->i_buf_cr += 2;
	if (this->i_buf_cr >= this->buf_cr_size)
		this->i_buf_cr -= this->buf_cr_size;
}

inline void PlotBuffer::buf_cr_spike_add(float x, float y)
{
	this->buf_cr_spike[this->i_buf_cr_spike].point.x = x;
	this->buf_cr_spike[this->i_buf_cr_spike].point.y = y;
	this->i_buf_cr_spike += 2;
}

inline unsigned int PlotBuffer::cur_move(unsigned int cur, int offset) const
{
	int cur_new = (int)cur + offset;
	while (cur_new < 0)
		cur_new += this->buf_cr_cnt_max;
	while (cur_new >= this->buf_cr_cnt_max)
		cur_new -= this->buf_cr_cnt_max;
	return cur_new;
}

inline unsigned int PlotBuffer::i_to_cur(unsigned int i) const
{
	return i / 2;
}

inline unsigned int PlotBuffer::cur_to_i(unsigned int cur) const
{
	return 2*cur + 1;
}

}
#endif

