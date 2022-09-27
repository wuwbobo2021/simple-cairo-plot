// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#include <chrono>

#include <simple-cairo-plot/plottingarea.h>

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
	
	bool except_caught = false;
	try {
		this->buf_spike = new unsigned long int[this->source->spike_buffer_size()];
		this->buf_cairo = new cairo_path_data_t[2*(this->source->size() + this->source->spike_buffer_size())];
	} catch (std::bad_alloc) {
		except_caught = true;
	}
	if (except_caught || this->buf_spike == NULL || this->buf_cairo == NULL) {
		if (this->buf_spike) {delete this->buf_spike; this->buf_spike = NULL;}
		throw std::bad_alloc();
	}
	
	this->dispatcher.connect(sigc::mem_fun(*(Gtk::Widget*)this, &Gtk::Widget::queue_draw));
	
	this->color_plot.set_rgba(1.0, 0.0, 0.0); //red
	this->oss.setf(std::ios::fixed);
}

PlottingArea::~PlottingArea()
{
	this->set_refresh_mode(false); //make sure the thread is ended
}

bool PlottingArea::set_refresh_mode(bool auto_refresh, unsigned int interval)
{
	if (this->source == NULL && auto_refresh)
		throw std::runtime_error("PlottingArea::set_refresh_mode(): pointer of source data buffer is not set.");
	
	if (interval > 0) {
		if (interval < 20) interval = 20; //maximum graph refresh rate: 50 Hz
		this->refresh_interval = interval;
	}
	
	if (auto_refresh == this->flag_auto_refresh) return true;
	this->flag_auto_refresh = auto_refresh;
	if (auto_refresh) {
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
	
	if (this->option_auto_goto_end) {
		if (this->option_auto_extend_range_x)
			this->range_x_extend();
		else
			this->range_x_goto_end();
	}
	
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
	range.set_int();
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

void PlottingArea::range_x_extend(bool remain_space)
{
	if (this->range_x.contain(this->source->range())) return;
	
	if (remain_space) {
		this->range_x.min_move_to(0);
		if (this->range_x.contain(this->source->range())) return;
		this->range_x.scale(2, 0);
		if (this->range_x.contain(this->source->range_max()))
			this->range_x = this->source->range_max();
		else
			this->range_x.fit_by_range(this->source->range_max());
	} else
		this->range_x = this->source->range();
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
	
	this->source->set_spike_check_ref_min(range_tight.center());
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

void PlottingArea::on_style_updated()
{
	Gdk::RGBA color_fore = this->get_style_context()->get_color();
	this->color_text = color_fore;

	if ((color_fore.get_red() + color_fore.get_green() + color_fore.get_blue()) / 3 > 0.5) //dark background
		this->color_grid.set_rgba(0.4, 0.4, 0.4); //deep gray
	else //light background
		this->color_grid.set_rgba(0.8, 0.8, 0.8); //light gray
}

bool PlottingArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	this->adjust_index_step();
	
	if (this->flag_check_range_y) {
		this->range_y_auto_set(this->flag_adapt);
		this->flag_adapt = this->flag_check_range_y = false;
	}
	
	Gtk::Allocation inner_alloc = this->draw_grid(cr);
	this->plot(cr, inner_alloc);
	if (this->option_show_average_line)
		this->draw_average_line(cr, inner_alloc);
	
	return true;
}

void PlottingArea::adjust_index_step()
{
	unsigned int plot_data_amount_max = 2 * this->get_allocation().get_width();
	if (plot_data_amount_max < Plot_Data_Amount_Limit_Min)
		plot_data_amount_max = Plot_Data_Amount_Limit_Min;
	
	this->index_step = 1;
	while (this->range_x.length() / this->index_step > plot_data_amount_max)
		this->index_step++;
}

inline unsigned int get_precision(float len_seg)
{
	if (len_seg == 0) return 0;
	float len = len_seg / 10.0; unsigned int i;
	for (i = 0; len < 1; len *= 10.0, i++);
	return i;
}

inline std::string float_to_str(float val, std::ostringstream& oss)
{
	oss.str(""); oss << val;
	return oss.str();
}

inline void set_cr_color(const Cairo::RefPtr<Cairo::Context>& cr, const Gdk::RGBA& color)
{
	cr->set_source_rgb(color.get_red(), color.get_green(), color.get_blue());
}

Gtk::Allocation PlottingArea::draw_grid(const Cairo::RefPtr<Cairo::Context>& cr)
{
	Gtk::Allocation alloc = this->get_allocation();
	float inner_x1 = (this->option_show_axis_y_values? Border_X_Left : 0),
	      inner_y1 = Border_Y,
	      inner_x2 = alloc.get_width(),
	      inner_y2 = alloc.get_height() - Border_Y;
	
	AxisRange alloc_x(inner_x1, inner_x2),
			  alloc_y(inner_y1, inner_y2);
	if (alloc_x.length() < 10 || alloc_y.length() < 10)
		return Gtk::Allocation(0, 0, 0, 0);
	
	AxisRange range_val_x = this->source->range_to_abs(this->range_x);
	range_val_x.scale(this->axis_x_unit, 0);
	
	AxisValues axis_x_values(range_val_x,   this->axis_x_divider, !this->option_fixed_scale),
			   axis_y_values(this->range_y, this->axis_y_divider, !this->option_fixed_scale);
	
	set_cr_color(cr, this->color_grid);
	
	// draw border
	cr->set_line_width(2.0);
	cr->rectangle(inner_x1 + 1.0, inner_y1 + 1.0,
	              inner_x2 - inner_x1 - 2.0, inner_y2 - inner_y1 - 2.0);
	cr->stroke();
	
	// draw grid
	float gx_cur, gy_cur;
	cr->set_line_width(1.0);
	for (unsigned int i = 0; i < axis_x_values.count(); i++) {
		gx_cur = range_val_x.map(axis_x_values[i], alloc_x);
		cr->move_to(gx_cur, inner_y1);
		cr->line_to(gx_cur, inner_y2);
	}
	for (unsigned int i = 0; i < axis_y_values.count(); i++) {
		gy_cur = this->range_y.map_reverse(axis_y_values[i], alloc_y);
		cr->move_to(inner_x1, gy_cur);
		cr->line_to(inner_x2, gy_cur);
	}
	cr->stroke();
	
	// print value labels for axis x, y
	if (this->option_show_axis_x_values || this->option_show_axis_y_values) {
		oss.clear();
		cr->set_font_size(12);
		set_cr_color(cr, this->color_text);
		
		if (this->option_show_axis_x_values) {
			if (this->option_axis_x_int_values)
				oss.precision(0);
			else
				oss.precision(get_precision(range_val_x.length() / this->axis_x_divider));
				
			float val; std::string str_x_val, str_x_val_prev = "";
			for (unsigned int i = 0; i < axis_x_values.count(); i++) {
				val = axis_x_values[i];
				gx_cur = range_val_x.map(val, alloc_x);
				if (inner_x2 - gx_cur < 50) break;
				str_x_val = float_to_str(val, this->oss);
				if (!this->option_axis_x_int_values || str_x_val != str_x_val_prev) {
					cr->move_to(gx_cur, inner_y2 + 10);
					cr->show_text(float_to_str(val, this->oss));
				}
				if (this->option_axis_x_int_values) str_x_val_prev = str_x_val;
			}
			
			// show axis x unit name
			if (this->axis_x_unit_name.length() > 0) {
				cr->move_to(inner_x2 - (this->axis_x_unit_name.length() + 2) * 5, inner_y2 + 10);
				cr->show_text('(' + this->axis_x_unit_name + ')');
			}
		}
		
		if (this->option_show_axis_y_values) {
			oss.precision(get_precision(this->range_y.length() / this->axis_y_divider));
			float val;
			for (unsigned int i = 0; i < axis_y_values.count(); i++) {
				val = axis_y_values[i];
				gy_cur = this->range_y.map_reverse(val, alloc_y);
				cr->move_to(0, gy_cur);
				if (i < axis_y_values.count() - 1 || this->axis_y_unit_name.length() == 0)
					cr->show_text(float_to_str(val, this->oss));
				else { // print topmost value with axis y unit name added
					gy_cur -= 2; cr->move_to(0, gy_cur);
					cr->show_text(float_to_str(val, this->oss) + '(' + this->axis_y_unit_name + ')');
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

	AxisRange range_x_abs = this->source->range_to_abs(this->range_x);
	unsigned int i_min = range_x_abs.min(), i_max = range_x_abs.max() + 1;
	if (i_max >= this->source->count_overall()) i_max--;
	
	AxisRange alloc_y(alloc.get_y(), alloc.get_y() + alloc.get_height());
	float w_cur = alloc.get_x(),
	      w_unit = alloc.get_width() * this->index_step / this->range_x.length();
	float val_first = this->source->item(this->range_x.min()),
	      h_cur = this->range_y.map_reverse(val_first, alloc_y);
	
	this->buf_cr_clear();
	this->buf_cr_add(w_cur, h_cur, true); i_min++;
	for (unsigned int i = i_min; i <= i_max; i += this->index_step) {
		w_cur += w_unit;
		h_cur = this->range_y.map_reverse(this->source->abs_index_item(i), alloc_y);
		this->buf_cr_add(w_cur, h_cur);
	}
	
	// draw spikes seperately when index_step > 1
	if (this->index_step > 1) {
		AxisRange alloc_x(alloc.get_x(), alloc.get_x() + alloc.get_width());
		w_unit = alloc.get_width() / this->range_x.length();
		unsigned int cnt_sp = this->source->get_spikes(this->range_x, this->buf_spike);
		
		for (unsigned int i_sp = 0, i; i_sp < cnt_sp; i_sp++) {
			i = this->source->index_to_rel(this->buf_spike[i_sp]);
			w_cur = this->range_x.map(i, alloc_x);
			h_cur = this->range_y.map_reverse(this->source->item(i), alloc_y);
			this->buf_cr_add(w_cur, h_cur, true);
			w_cur += w_unit;
			h_cur = this->range_y.map_reverse(this->source->item(i + 1), alloc_y);
			this->buf_cr_add(w_cur, h_cur);
		}
	}
	
	cairo_path_t path_info = {CAIRO_STATUS_SUCCESS, this->buf_cairo, (int)this->i_buf_cairo};
	cairo_append_path(cr->cobj(), &path_info);
	
	cr->set_line_width(1.0);
	set_cr_color(cr, this->color_plot);
	if (this->option_anti_alias)
		cr->set_antialias(Cairo::ANTIALIAS_GRAY);
	else
		cr->set_antialias(Cairo::ANTIALIAS_NONE);
	cr->stroke();
}

void PlottingArea::draw_average_line(const Cairo::RefPtr<Cairo::Context>& cr, Gtk::Allocation alloc)
{
	AxisRange alloc_y(alloc.get_y(), alloc.get_y() + alloc.get_height());
	
	float av = this->source->get_average(this->range_x, this->index_step);
	float y_cur = this->range_y.map_reverse(av, alloc_y);
	
	cr->set_line_width(1.0);
	set_cr_color(cr, this->color_text);
	cr->set_dash(this->dash_pattern, 0);
	
	cr->move_to(alloc.get_x(), y_cur);
	cr->line_to(alloc.get_x() + alloc.get_width(), y_cur);
	cr->stroke();
	
	cr->unset_dash();
}
