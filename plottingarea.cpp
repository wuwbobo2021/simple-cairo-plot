// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include <chrono>

#include "plottingarea.h"

using namespace SimpleCairoPlot;

PlottingArea::PlottingArea() {}

PlottingArea::PlottingArea(CircularBuffer* buf)
{
	this->init(buf);
}

void PlottingArea::init(CircularBuffer* buf)
{
	if (this->flag_auto_refresh) this->set_refresh_mode(false);
	
	if (! buf)
		throw std::invalid_argument("PlottingArea::init(): the buffer pointer is null.");
	this->source = buf;
	this->dispatcher.connect(sigc::mem_fun(*(Gtk::Widget*)this, &Gtk::Widget::queue_draw));
	
	this->color_plot.set_rgba(1.0, 0.0, 0.0); //red
}

PlottingArea::~PlottingArea()
{
	this->set_refresh_mode(false);
}

bool PlottingArea::set_refresh_mode(bool auto_refresh, unsigned int interval)
{
	if (this->source == NULL && auto_refresh)
		throw std::runtime_error("PlottingArea::set_refresh_mode(): pointer of source data buffer is not set.");
	
	if (auto_refresh == this->flag_auto_refresh) return true;
	this->flag_auto_refresh = auto_refresh;
	
	if (auto_refresh) {
		if (interval > 0) this->refresh_interval = interval;
		try {
			this->thread_timer = new std::thread(&PlottingArea::refresh_loop, this);
			return true;
		} catch (std::exception ex) {
			this->flag_auto_refresh = false;
			this->thread_timer = NULL;
			return false;
		}
	} else {
		if (! this->thread_timer) return true;
		this->thread_timer->join();
		delete(this->thread_timer);
		return true;
	}
}

void PlottingArea::refresh(bool forced_check_range_y, bool forced_adapt)
{
	if (! this->source)
		throw std::runtime_error("PlottingArea::refresh(): pointer of source data buffer is not set.");
	
	if (this->option_auto_goto_end) this->range_x_goto_end();
	
	if (forced_check_range_y) this->flag_check_range_y = true;
	else if (this->option_auto_set_range_y)
		if (++this->counter1 > 5) {
			this->flag_check_range_y = true; this->counter1 = 0;
		}
	
	if (this->flag_check_range_y) {
		if (forced_adapt) this->flag_adapt = true;
		else if (++this->counter2 > 5) {
			this->flag_adapt = true; this->counter2 = 0;
		}
	}
	
	this->dispatcher.emit(); //let the main thread enter this->queue_draw() and draw the frame
}

AxisRange PlottingArea::get_range_x() const
{
	return this->range_x;
}

AxisRange PlottingArea::get_range_y() const
{
	return this->range_y;
}

bool PlottingArea::set_axis_divider(unsigned int x_div, unsigned int y_div)
{
	if (x_div == 0 && y_div == 0) return false;
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

bool PlottingArea::set_axis_y_range_length_min(float length_min)
{
	if (length_min < 0) return false;
	this->axis_y_length_min = length_min;
	return true;
}

bool PlottingArea::set_range_x(AxisRange range)
{
	if (range.length() == 0) return false;
	if (! this->source->is_valid_range(range)) return false;
	this->range_x = range;
	this->adjust_index_step();
	return true;
}

void PlottingArea::range_x_goto_end()
{
	if (this->source->count() >= this->range_x.length() + 1)
		this->range_x.max_move_to(this->source->count() - 1);
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

void PlottingArea::range_y_auto_set(bool adapt)
{
	if (this->source->count() <= 1) {
		this->range_y.set(0, 10); return;
	}
	
	AxisRange range_tight = this->source->get_value_range(this->range_x, this->index_step);
	if (adapt == false && this->range_y.contain(range_tight)) return;
	
	float min = range_tight.min(), max = range_tight.max();
	
	if (min < 0 || this->option_auto_set_zero_bottom == false) {
		if (max > min) {
			this->range_y.set(min, max); this->range_y.scale(1.2);
		} else
			this->range_y.set(min - 0.2*min, min + 0.2*min); //max = min < 0, rare
		
		if (this->range_y.length() < this->axis_y_length_min)
			this->range_y.scale(this->axis_y_length_min / this->range_y.length());
		
		if (min >= 0 && this->range_y.min() < 0)
			this->range_y.min_move_to(0);
	} else {
		// without any minus value, always set lower bound to 0
		if (max > 0)
			this->range_y.set(0, 1.2*max);
		else
			this->range_y.set(0, 10); //min = max = 0, rare
		
		if (this->range_y.length() < this->axis_y_length_min)
			this->range_y.scale(this->axis_y_length_min / this->range_y.length(), 0);
	}
}

/*------------------------------ private functions ------------------------------*/

void PlottingArea::refresh_loop() //in the timer thread
{
	using namespace std::chrono;
	using namespace std::this_thread;
	
	while (this->flag_auto_refresh) {
		steady_clock::time_point time_bef_draw = steady_clock::now();
		this->refresh(); 
		sleep_until(time_bef_draw + milliseconds(this->refresh_interval));
	}
}

bool PlottingArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	this->adjust_index_step();
	
	if (this->flag_check_range_y) {
		this->range_y_auto_set(this->flag_adapt);
		this->flag_adapt = this->flag_check_range_y = false;
	}
	
	this->plot(cr, this->draw_grid(cr));
	return true;
}

void PlottingArea::adjust_index_step()
{
	unsigned int plot_data_amount_max = 4 * this->get_allocation().get_width();
	if (plot_data_amount_max < 1024)
		plot_data_amount_max = 1024;
	
	this->index_step = 1;
	while (this->range_x.length() / this->index_step > plot_data_amount_max)
		this->index_step++;
}

inline std::string float_to_str(float val, std::stringstream& sst)
{
	sst.str(""); sst << val;
	return sst.str();
}

inline void set_cr_color(const Cairo::RefPtr<Cairo::Context>& cr, const Gdk::RGBA& color)
{
	cr->set_source_rgb(color.get_red(), color.get_green(), color.get_blue());
}

void PlottingArea::on_style_updated()
{
	Gdk::RGBA color_fore = this->get_style_context()->get_color();
	this->color_text = color_fore;
	
	if ((color_fore.get_red() + color_fore.get_green() + color_fore.get_blue()) / 3 > 0.5) //dark background
		this->color_grid.set_rgba(0.4, 0.4, 0.4); //deep gray
	else //light background
		this->color_grid.set_rgba(0.8, 0.8, 0.8); //light gray
}

Gtk::Allocation PlottingArea::draw_grid(const Cairo::RefPtr<Cairo::Context>& cr)
{
	Gtk::Allocation alloc = this->get_allocation();
	float inner_x1 = (this->option_show_axis_y_values? Border_X_Left : 0),
	      inner_y1 = Border_Y,
	      inner_x2 = alloc.get_width(),
	      inner_y2 = alloc.get_height() - Border_Y;
	
	if (inner_x2 - inner_x1 < 10 || inner_y2 - inner_y1 < 10)
		return Gtk::Allocation(0, 0, 0, 0);
	
	set_cr_color(cr, this->color_grid);
	
	// draw border
	cr->set_line_width(2.0);
	cr->rectangle(inner_x1 + 1.0, inner_y1 + 1.0,
	              inner_x2 - inner_x1 - 2.0, inner_y2 - inner_y1 - 2.0);
	cr->stroke();
	
	//draw grid
	float gx_cur, gy_cur;
	cr->set_line_width(1.0);
	
	// draw vertical lines for axis x
	AxisRange range_grid_x(0, this->axis_x_divider),
	          alloc_x(inner_x1, inner_x2);
	for (unsigned int i = 1; i < this->axis_x_divider; i++) {
		gx_cur = range_grid_x.map(i, alloc_x);
		cr->move_to(gx_cur, inner_y1);
		cr->line_to(gx_cur, inner_y2);
	}
	
	// draw horizontal lines for axis y
	AxisRange range_grid_y(0, this->axis_y_divider),
	          alloc_y(inner_y1, inner_y2);
	for (unsigned int i = 1; i < this->axis_y_divider; i++) {
		gy_cur = range_grid_y.map_reverse(i, alloc_y);
		cr->move_to(inner_x1, gy_cur);
		cr->line_to(inner_x2, gy_cur);
	}
	
	cr->stroke();
	
	// print value labels for axis x, y
	if (this->option_show_axis_x_values || this->option_show_axis_y_values) {
		sst.clear();
		set_cr_color(cr, this->color_text);
		
		if (this->option_show_axis_x_values) {
			sst.unsetf(std::ios::fixed); this->sst.precision(6);
			AxisRange range_val_x = this->source->range_to_abs(this->range_x);
			range_val_x.scale(this->axis_x_unit, 0);
			
			for (unsigned int i = 0; i <= this->axis_x_divider; i++) {
				gx_cur = range_grid_x.map(i, alloc_x);
				float val = range_grid_x.map(i, range_val_x);
				cr->move_to(gx_cur, inner_y2 + 10);
				cr->show_text(float_to_str(val, this->sst));
			}
			
			if (this->axis_x_unit_name.length() > 0) {
				cr->move_to(inner_x2 - (this->axis_x_unit_name.length() + 2) * 4.5, inner_y2 + 10);
				cr->show_text('(' + this->axis_x_unit_name + ')');
			}
		}
		
		if (this->option_show_axis_y_values) {
			sst.setf(std::ios::fixed); sst.precision(3);
			for (unsigned int i = 0; i <= this->axis_y_divider; i++) {
				gy_cur = range_grid_y.map_reverse(i, alloc_y);
				float val = range_grid_y.map(i, this->range_y);
				cr->move_to(0, gy_cur);
				if (i < this->axis_y_divider || this->axis_y_unit_name.length() == 0)
					cr->show_text(float_to_str(val, this->sst));
				else {
					gy_cur -= 2; cr->move_to(0, gy_cur);
					cr->show_text(float_to_str(val, this->sst) + '(' + this->axis_y_unit_name + ')');
				}
			}
		}
		
		cr->stroke();
	}
	
	return Gtk::Allocation(inner_x1, inner_y1, inner_x2 - inner_x1, inner_y2 - inner_y1);
}

void PlottingArea::plot(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc)
{
	if (alloc.has_zero_area()) return;
	if (this->source->count() < 2) return;
	
	cr->set_line_width(1.0);
	set_cr_color(cr, this->color_plot);
	if (this->option_anti_alias)
		cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	else
		cr->set_antialias(Cairo::ANTIALIAS_NONE);
	
	AxisRange alloc_y(alloc.get_y(), alloc.get_y() + alloc.get_height());
	float w_cur = alloc.get_x(),
	      w_unit = alloc.get_width() * this->index_step / this->range_x.length();
	float val_first = (*this->source)[this->range_x.min()];
	cr->move_to(w_cur, this->range_y.map_reverse(val_first, alloc_y));
	
	for (unsigned int i = this->range_x.min() + 1;
	                  i <= this->range_x.max() + 1 && i < this->source->count();
	                  i += this->index_step) {
		w_cur += w_unit;
		cr->line_to(w_cur, this->range_y.map_reverse((*this->source)[i], alloc_y));
	}
	
	cr->stroke();
}

