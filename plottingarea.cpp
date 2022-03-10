// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include <string>
#include <sstream>
#include <stdexcept>
#include <chrono>

#include "plottingarea.h"

using namespace SimpleCairoPlot;

const unsigned int Border_X_Left = 40, Border_Y = 10;

PlottingArea::PlottingArea(CircularBuffer& buf):
	source(buf)
{
	this->dispatcher.connect(sigc::mem_fun(*this, &PlottingArea::refresh_callback));
	
	this->option_auto_refresh = false;
	this->set_refresh_mode(true);
	
	this->color_plot.set_rgb(1.0, 0.0, 0.0); //red
}

PlottingArea::~PlottingArea()
{
	this->set_refresh_mode(false);
}

bool PlottingArea::set_range_x(AxisRange range)
{
	if (range.length() == 0) return false;
	if (! this->source.is_valid_range(range)) return false;
	this->range_x = range; return true;
}

void PlottingArea::range_x_goto_end()
{
	if (this->source.count() >= this->range_x.length() + 1)
		this->range_x.max_move_to(this->source.count() - 1);
	else
		this->range_x.min_move_to(0);
}

bool PlottingArea::set_range_y(AxisRange range)
{
	if (range.length() == 0) return false;
	if (! this->option_auto_set_range_y) {
		this->range_y = range; return true;
	} else return false;
}

void PlottingArea::range_y_auto_set(bool tight)
{
	if (this->source.count() <= 1)
		this->range_y.set(0, 10);
	else {
		AxisRange range_tight = this->source.get_value_range(this->range_x);
		if (tight == false && this->range_y.contain(range_tight)) return;
		
		float min = range_tight.min(), max = range_tight.max();
		
		if (min < 0) {
			if (max > min) {
				float mid = (max + min) / 2, half_diff = (max - min) / 2;
				this->range_y.set(mid - 1.25*half_diff, mid + 1.25*half_diff);
			} else
				//max = min < 0, rare
				this->range_y.set(min - 0.25*min, min + 0.25*min);
		} else {
			// without any minus value, always set lower bound to 0
			if (max > 0)
				this->range_y.set(0, 1.25*max);
			else
				//min = max = 0, rare
				this->range_y.set(0, 10);
		}
	}
}

bool PlottingArea::set_axis_divider(unsigned int x_div, unsigned int y_div)
{
	if (x_div > 100 || y_div > 100)
		throw std::invalid_argument("PlottingArea::set_axis_divider(): value of divider is too large.");
	if (x_div == 0) x_div = 1; if (y_div == 0) y_div = 1;
	
	this->axis_x_divider = x_div;
	this->axis_y_divider = y_div;
	return true;
}

bool PlottingArea::set_axis_x_unit(float unit)
{
	if (unit <= 0) return false;
	this->axis_x_unit = unit;
	return true;
}

void PlottingArea::set_refresh_mode(bool auto_refresh, unsigned int interval)
{
	if (auto_refresh == this->option_auto_refresh) return;
	
	if (auto_refresh) {
		if (interval > 0) this->refresh_interval = interval;
		this->thread_timer = new std::thread(&PlottingArea::refresh_loop, this);
	} else {
		this->flag_stop_auto_refresh = true;
		this->thread_timer->join();
		delete(this->thread_timer);
	}
}

void PlottingArea::refresh()
{
	static unsigned int counter1, counter2;
	
	if (this->option_auto_goto_end) this->range_x_goto_end();
	
	if (this->option_auto_set_range_y) {
		bool auto_set_basic = false, auto_set_adapt = false;
		if (++counter1 > 5) {
			auto_set_basic = true; counter1 = 0;
			if (++counter2 > 5) {
				auto_set_adapt = true; counter2 = 0;
			}
		}
		if (auto_set_basic)
			this->range_y_auto_set(auto_set_adapt);
	}
	this->dispatcher.emit(); //let the main thread draw the frame
}

/*------------------------------ private functions ------------------------------*/

void PlottingArea::refresh_loop() //in the timer thread
{
	using namespace std::chrono;
	using namespace std::this_thread;
	
	while (! this->flag_stop_auto_refresh) {
		time_point time_bef_draw = system_clock::now();
		this->refresh(); 
		sleep_until(time_bef_draw + milliseconds(this->refresh_interval));
	}
	
	this->flag_stop_auto_refresh = false;
}

void PlottingArea::refresh_callback() //in the main thread
{
	this->queue_draw(); //redraw request, calls on_draw()
}

bool PlottingArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	this->plot(cr, this->draw_grid(cr, this->get_allocation()));
	return true;
}

inline std::string float_to_str(float val, std::stringstream& sst)
{
	std::string str;
	sst.clear(); sst << val; sst >> str;
	return str;
}

Gtk::Allocation PlottingArea::draw_grid(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc)
{
	// pair (x, y) of alloc should be (0, 0)
	float inner_x1 = (this->option_show_axis_y_values? Border_X_Left : 0),
	      inner_y1 = Border_Y,
	      inner_x2 = alloc.get_width(),
	      inner_y2 = alloc.get_height() - Border_Y;
	
	if (inner_x2 - inner_x1 < 10 || inner_y2 - inner_y1 < 10)
		return Gtk::Allocation(0, 0, 0, 0);
	
	cr->set_line_width(1.0);
	cr->set_source_rgb(0.8, 0.8, 0.8); //gray
	
	float gx_cur, gy_cur;
	
	// draw vertical lines for axis x
	AxisRange range_grid_x(0, this->axis_x_divider),
	          alloc_x(inner_x1, inner_x2);
	for (unsigned int i = 0; i <= this->axis_x_divider; i++) {
		gx_cur = range_grid_x.map(i, alloc_x);
		cr->move_to(gx_cur, inner_y1);
		cr->line_to(gx_cur, inner_y2);
	}
	
	// draw horizontal lines for axis y
	AxisRange range_grid_y(0, this->axis_y_divider),
	          alloc_y(inner_y1, inner_y2);
	for (unsigned int i = 0; i <= this->axis_y_divider; i++) {
		gy_cur = range_grid_y.map(i, alloc_y, true);
		cr->move_to(inner_x1, gy_cur);
		cr->line_to(inner_x2, gy_cur);
	}
	
	cr->stroke();
	
	// print value labels for axis x, y
	if (this->option_show_axis_x_values || this->option_show_axis_y_values) {
		cr->set_source_rgb(0.0, 0.0, 0.0); //black
		std::stringstream sst; sst.precision(6);
		
		if (this->option_show_axis_x_values) {
			AxisRange range_val_x(this->range_x);
			range_val_x.move(this->source.count_discarded());
			range_val_x.scale(this->axis_x_unit, 0);
			
			for (unsigned int i = 0; i <= this->axis_x_divider; i++) {
				gx_cur = range_grid_x.map(i, alloc_x);
				float val = range_grid_x.map(i, range_val_x);
				cr->move_to(gx_cur, inner_y2 + 10);
				cr->show_text(float_to_str(val, sst));
			}
		}
		
		if (this->option_show_axis_y_values) {
			sst.setf(std::ios::fixed); sst.precision(3);
			for (unsigned int i = 0; i <= this->axis_y_divider; i++) {
				gy_cur = range_grid_y.map(i, alloc_y, true);
				float val = range_grid_y.map(i, this->range_y);
				cr->move_to(0, gy_cur);
				cr->show_text(float_to_str(val, sst));
			}
		}
		
		cr->stroke();
	}
	
	return Gtk::Allocation(inner_x1, inner_y1, inner_x2 - inner_x1, inner_y2 - inner_y1);
}

void PlottingArea::plot(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc)
{
	if (alloc.has_zero_area()) return;
	if (this->source.count() < 2) return;
	
	cr->set_line_width(1.0);
	cr->set_source_rgb(this->color_plot.get_red(), this->color_plot.get_green(),
	                   this->color_plot.get_blue());
	if (this->option_anti_alias)
		cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	else
		cr->set_antialias(Cairo::ANTIALIAS_NONE);
	
	AxisRange alloc_y(alloc.get_y(), alloc.get_y() + alloc.get_height());
	float w_cur = alloc.get_x(),
	      w_unit = alloc.get_width() / this->range_x.length();
	cr->move_to(w_cur, this->range_y.map(this->source[this->range_x.min()], alloc_y, true));
	
	for (unsigned int i = this->range_x.min() + 1;
	                  i <= this->range_x.max() && i < this->source.count();
	                  i ++) {
		w_cur += w_unit;
		cr->line_to(w_cur, this->range_y.map(this->source[i], alloc_y, true));
	}
	
	cr->stroke();
}

