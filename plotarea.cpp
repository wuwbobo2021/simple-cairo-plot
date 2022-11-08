// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#include <simple-cairo-plot/plotarea.h>

#include <chrono>
#include <gdkmm/drawingcontext.h>
#include <gtkmm/container.h>

using namespace SimpleCairoPlot;

PlotArea::PlotArea() {}

PlotArea::PlotArea(CircularBuffer* buf)
{
	this->init(buf);
}

void PlotArea::init(CircularBuffer* buf)
{
	if (this->flag_auto_refresh) this->set_refresh_mode(false);
	
	if (! buf)
		throw std::invalid_argument("PlotArea::init(): the buffer pointer is null.");
	this->source = buf;
	
	unsigned int limit_max = 2 * this->get_screen()->get_monitor_workarea().get_width();
	if (limit_max > this->source->size()) limit_max = this->source->size();
	this->plot_data_amount_max_range.set(Plot_Data_Amount_Limit_Min, limit_max);
	
	this->buf_plot.init(buf, limit_max);
	
	this->signal_size_allocate().connect(sigc::mem_fun(*this, &PlotArea::on_size_allocation));
	this->dispatcher.connect(sigc::bind(sigc::mem_fun(*this, &PlotArea::draw),
	                                    (Cairo::RefPtr<Cairo::Context>)nullptr));
	
	this->param.color_plot.set_rgba(1.0, 0.0, 0.0); //red
	this->oss.setf(std::ios::fixed);
}

PlotArea::~PlotArea()
{
	this->set_refresh_mode(false); //make sure the thread is ended
}

bool PlotArea::set_refresh_mode(bool auto_refresh, unsigned int interval)
{
	if (this->source == NULL && auto_refresh)
		throw std::runtime_error("PlotArea::set_refresh_mode(): pointer of source data buffer is not set.");
	
	if (interval > 0) {
		if (interval < 40) interval = 40; //maximum graph refresh rate: 25 Hz
		this->refresh_interval = interval;
	}
	
	if (auto_refresh == this->flag_auto_refresh) return true;
	this->flag_auto_refresh = auto_refresh;
	if (auto_refresh) {
		try {
			this->thread_timer = new std::thread(&PlotArea::refresh_loop, this);
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

void PlotArea::refresh(bool forced_check_range_y, bool forced_adapt, bool forced_sync)
{
	if (! this->source)
		throw std::runtime_error("PlotArea::refresh(): not initialized.");
	
	if (forced_sync) this->flag_sync = true;
	if (flag_drawing) return;
	
	if (this->option_auto_goto_end) {
		if (this->option_auto_extend_range_x)
			this->range_x_extend();
		else
			this->range_x_goto_end();
	}
	
	if (this->option_auto_set_range_y) {
		if (forced_check_range_y) this->flag_check_range_y = true;
		else if (++this->counter1 > 5) {
			this->flag_check_range_y = true; this->counter1 = 0;
		}
	}
	if (this->flag_check_range_y) {
		if (forced_adapt) this->flag_adapt = true;
		else if (++this->counter2 > 5) {
			this->flag_adapt = true; this->counter2 = 0;
		}
	}
	
	this->dispatcher.emit(); //let the main thread draw the frame
}

bool PlotArea::set_range_x(IndexRange range)
{
	if (!range || range.count() < 2) return false;
	if (! this->source->is_valid_range(range)) return false;
	
	this->range_x = range;
	this->adjust_index_step();
	return true;
}

bool PlotArea::set_range_y_length_min(float length_min)
{
	if (length_min < 0) return false;
	this->range_y_length_min = length_min;
	return true;
}

void PlotArea::set_option_auto_goto_end(bool set)
{
	this->option_auto_goto_end = set;
}
void PlotArea::set_option_auto_extend_range_x(bool set)
{
	this->option_auto_extend_range_x = set;
}
void PlotArea::set_option_auto_set_range_y(bool set)
{
	this->option_auto_set_range_y = set;
	if (! set) this->flag_check_range_y = false;
}
void PlotArea::set_option_auto_set_zero_bottom(bool set)
{
	this->option_auto_set_zero_bottom = set;
}

void PlotArea::range_x_goto_end()
{
	if (this->source->count() >= this->range_x.count())
		this->range_x.max_move_to(this->source->count() - 1);
	else
		this->range_x.min_move_to(0);
}

void PlotArea::range_x_extend(bool remain_space)
{
	if (this->range_x.contain(this->source->range())) return;
	
	if (remain_space) {
		this->range_x.min_move_to(0);
		if (this->range_x.contain(this->source->range())) return;
		this->range_x.set(0, 2*this->range_x.count() - 1);
		if (this->range_x.contain(this->source->range_max()))
			this->range_x = this->source->range_max();
		else
			this->range_x.fit_by_range(this->source->range_max());
	} else
		this->range_x = this->source->range();
}

bool PlotArea::set_range_y(ValueRange range)
{
	if (range.length() == 0) return false;
	if (! this->option_auto_set_range_y) {
		this->param.range_y = range; return true;
	} else return false;
}

void PlotArea::range_y_auto_set(bool adapt)
{
	if (this->source->count() <= 1) {
		this->param.range_y.set(0, 10); return;
	}
	
	ValueRange range_tight = this->source->get_value_range(this->range_x, this->param.index_step);
	if (adapt == false && this->param.range_y.contain(range_tight)) return;
	
	float min = range_tight.min(), max = range_tight.max();
	
	if (min < 0 || this->option_auto_set_zero_bottom == false) {
		if (max > min) {
			this->param.range_y.set(min, max); this->param.range_y.scale(1.2);
		} else
			this->param.range_y.set(min - 0.2*min, min + 0.2*min); //max = min < 0, rare
		
		if (this->param.range_y.length() < this->range_y_length_min)
			this->param.range_y.scale(this->range_y_length_min / this->param.range_y.length());
		
		if (min >= 0 && this->param.range_y.min() < 0)
			this->param.range_y.min_move_to(0);
	} else {
		// without any minus value, always set lower bound to 0
		if (max > 0)
			this->param.range_y.set(0, 1.2*max);
		else
			this->param.range_y.set(0, 10); //min = max = 0, rare
		
		if (this->param.range_y.length() < this->range_y_length_min)
			this->param.range_y.scale(this->range_y_length_min / this->param.range_y.length(), 0);
	}
	
	this->source->set_spike_check_ref_min(range_tight.center());
}

bool PlotArea::set_axis_divider(unsigned int x_div, unsigned int y_div)
{
	if (x_div == 0 && y_div == 0) return false;
	if (x_div == 0) x_div = 1; if (y_div == 0) y_div = 1;
	
	this->param.axis_x_divider = x_div;
	this->param.axis_y_divider = y_div;
	return true;
}

bool PlotArea::set_axis_x_unit(float unit)
{
	if (unit <= 0) return false;
	this->param.axis_x_unit = unit;
	return true;
}

void PlotArea::set_axis_x_unit_name(std::string str_unit)
{
	this->param.axis_x_unit_name = str_unit;
}

void PlotArea::set_axis_y_unit_name(std::string str_unit)
{
	this->param.axis_y_unit_name = str_unit;
}

void PlotArea::set_option_fixed_scale(bool set)
{
	this->param.option_fixed_scale = set;
}

void PlotArea::set_option_show_axis_x_values(bool set)
{
	this->param.option_show_axis_x_values = set;
}

void PlotArea::set_option_axis_x_int_values(bool set)
{
	this->param.option_axis_x_int_values = set;
}

void PlotArea::set_option_show_axis_y_values(bool set)
{
	this->param.option_show_axis_y_values = set;
}

void PlotArea::set_option_show_average_line(bool set)
{
	this->param.option_show_average_line = set;
}

void PlotArea::set_plot_color(Gdk::RGBA color)
{
	this->param.color_plot = color;
}

void PlotArea::set_option_anti_alias(bool set)
{
	this->param.option_anti_alias = set;
}

/*------------------------------ private functions ------------------------------*/

void PlotArea::on_style_updated()
{
	if (! flag_set_colors) return;
	
	Gdk::RGBA color_fore = this->get_style_context()->get_color();
	this->color_text = color_fore;
	
	if ((color_fore.get_red() + color_fore.get_green() + color_fore.get_blue()) / 3 < 0.5) { //light background
		this->color_back.set_rgba(1.0, 1.0, 1.0); //white
		this->color_grid.set_rgba(0.8, 0.8, 0.8); //light gray
	} else {
		this->color_back.set_rgba(0.1, 0.1, 0.1); //black
		this->color_grid.set_rgba(0.4, 0.4, 0.4); //deep gray
	}
	flag_set_colors = false;
}

void PlotArea::on_size_allocation(Gtk::Allocation& allocation)
{	
	// param.alloc is the area for plotting; alloc_outer might contain tick values.
	this->param.alloc_outer = Gtk::Allocation(0, 0, allocation.get_width(), allocation.get_height());
	unsigned int border_x_left = (this->param.option_show_axis_y_values? this->Border_X_Left : 0);
	this->param.alloc = Gtk::Allocation(border_x_left, this->Border_Y,
                                        allocation.get_width() - border_x_left,
                                        allocation.get_height() - 2*this->Border_Y);
	this->adjust_index_step();
}

void PlotArea::adjust_index_step()
{
	unsigned int plot_data_amount_max =
		this->plot_data_amount_max_range.fit_value(2 * this->param.alloc.get_width());
	
	this->param.index_step = 1;
	while (ceil(this->range_x.count() / this->param.index_step) > plot_data_amount_max)
		this->param.index_step++;
}

bool PlotArea::on_draw(const Cairo::RefPtr<Cairo::Context>& cr)
{
	this->draw(cr);
	return true;
}

void PlotArea::refresh_loop() //in the timer thread
{
	using namespace std::chrono;
	using namespace std::this_thread;
	
	while (this->flag_auto_refresh) {
		steady_clock::time_point time_bef_draw = steady_clock::now();
		this->refresh(); 
		sleep_until(time_bef_draw + milliseconds(this->refresh_interval));
	}
}

static inline void set_cr_color(const Cairo::RefPtr<Cairo::Context>& cr, const Gdk::RGBA& color)
{
	cr->set_source_rgb(color.get_red(), color.get_green(), color.get_blue());
}

void PlotArea::draw(Cairo::RefPtr<Cairo::Context> cr)
{
	Gtk::Allocation alloc = this->param.alloc_outer;
	if (alloc.get_width() < 10 || alloc.get_height() < 10) return;
	
	this->flag_drawing = true;
	
	// refresh PlotParam
	this->param.data_cnt = this->source->count();
	this->param.data_cnt_overall = this->source->count_overall();
	this->param.range_x = this->source->range_to_abs(this->range_x);
	if (this->flag_check_range_y) { //this flag can be set by refresh()
		this->range_y_auto_set(this->flag_adapt);
		this->flag_adapt = this->flag_check_range_y = false;
	}
	if (this->param.option_show_average_line) {
		float av = this->source->get_average(this->range_x, this->param.index_step);
		this->param.y_av_alloc = this->param.range_y.map_reverse(av, this->param.alloc_y());
	}
	
	bool flag_clean = (bool)cr; //if cr is valid, it's passed from on_draw()
	bool flag_redraw = (   flag_clean || this->flag_sync
	                    || !this->param.reuse_graph(this->buf_plot.get_param()));
	
	Glib::RefPtr<Gdk::DrawingContext> drawing_context;
	if (! flag_clean) {
		// create cairo context (optimized). the frame isn't double-buffered because this
		// is not a top-level Gdk::Window (see reference of Gdk::Window::begin_draw_frame()).
		Glib::RefPtr<Gdk::Window> gdk_window = this->get_window();
		if (! gdk_window) return; //trying to avoid occasional segfault on Windows
		cairo_rectangle_int_t rect = {0, 0, (int)alloc.get_width(), (int)alloc.get_height()};
		drawing_context = gdk_window->begin_draw_frame(Cairo::Region::create(rect));
		if (drawing_context) cr = drawing_context->get_cairo_context();
	}
	if (! cr) return;
	
	if (flag_clean || this->flag_sync) {
		// fill back color even if flag_clean is set, because the widget's default color isn't known...
		set_cr_color(cr, this->color_back); cr->paint();
	} else if (flag_redraw) {
		// do erasing instead of filling with back color to reduce CPU usage
		set_cr_color(cr, this->color_back); cr->set_antialias(Cairo::ANTIALIAS_NONE);
		cr->set_line_width(this->buf_plot.get_param().option_anti_alias? 2.0 : 1.0);
		this->buf_plot.cairo_load(cr, true); cr->stroke();
		this->draw_grid(cr, this->buf_plot.get_param(), false);
	}
	
	if (flag_redraw)
		this->draw_grid(cr, this->param);
	
	set_cr_color(cr, this->param.color_plot); cr->set_line_width(1.0);
	cr->set_antialias(this->param.option_anti_alias? Cairo::ANTIALIAS_GRAY : Cairo::ANTIALIAS_NONE);
	this->buf_plot.sync(this->param, this->flag_sync);
	this->buf_plot.cairo_load(cr, flag_redraw); cr->stroke();
	
	if (! flag_clean) this->get_window()->end_draw_frame(drawing_context);
	this->flag_sync = false;
	this->flag_drawing = false;
}

static inline unsigned int get_precision(float len_seg)
{
	if (len_seg == 0) return 0;
	float len = len_seg / 10.0; unsigned int i;
	for (i = 0; len < 1; len *= 10.0, i++);
	return i;
}

static inline std::string float_to_str(float val, std::ostringstream& oss)
{
	oss.str(""); oss << val;
	return oss.str();
}

void PlotArea::draw_grid(Cairo::RefPtr<Cairo::Context> cr, const PlotParam& param, bool not_erase)
{
	float inner_x1 = param.alloc.get_x(),
	      inner_y1 = param.alloc.get_y();
	float inner_x2 = inner_x1 + param.alloc.get_width(),
	      inner_y2 = inner_y1 + param.alloc.get_height();
	
	AxisRange alloc_x(inner_x1, inner_x2),
			  alloc_y(inner_y1, inner_y2);
	
	AxisRange range_val_x = param.range_x;
	range_val_x.scale(param.axis_x_unit, 0);
	
	AxisValues axis_x_values(range_val_x,   param.axis_x_divider, !param.option_fixed_scale),
			   axis_y_values(param.range_y, param.axis_y_divider, !param.option_fixed_scale);
	
	set_cr_color(cr, not_erase? this->color_grid : this->color_back);
	cr->set_antialias(Cairo::ANTIALIAS_NONE);
	
	// draw border
	cr->set_line_width(2.0);
	cr->rectangle(inner_x1 + 1.0, inner_y1 + 1.0,
	              inner_x2 - inner_x1 - 3.0, inner_y2 - inner_y1 - 3.0);
	cr->stroke();
	
	// draw grid
	float x, y; cr->set_line_width(1.0);
	for (unsigned int i = 0; i < axis_x_values.count(); i++) {
		x = range_val_x.map(axis_x_values[i], alloc_x);
		cr->move_to(x, inner_y1);
		cr->line_to(x, inner_y2);
	}
	for (unsigned int i = 0; i < axis_y_values.count(); i++) {
		y = param.range_y.map_reverse(axis_y_values[i], alloc_y);
		cr->move_to(inner_x1, y);
		cr->line_to(inner_x2, y);
	}
	cr->stroke();
	
	if (param.option_show_average_line) {
		y = param.y_av_alloc;
		if (not_erase) {
			set_cr_color(cr, this->color_text);
			cr->set_dash(this->dash_pattern, 0);
		}
		cr->move_to(inner_x1, y);
		cr->line_to(inner_x2, y);
		cr->stroke(); cr->unset_dash();
	}
	
	// print value labels for axis x, y
	
	if (not_erase && (param.option_show_axis_x_values || param.option_show_axis_y_values)) {
		oss.clear(); cr->set_font_size(12); set_cr_color(cr, this->color_text);
	}
	
	if (param.option_show_axis_x_values) {
		if (not_erase) {
			if (param.option_axis_x_int_values)
				oss.precision(0);
			else
				oss.precision(get_precision(range_val_x.length() / param.axis_x_divider));
			
			float val; std::string str_x_val, str_x_val_prev = "";
			for (unsigned int i = 0; i < axis_x_values.count(); i++) {
				val = axis_x_values[i];
				x = range_val_x.map(val, alloc_x);
				if (inner_x2 - x < 50) break;
				str_x_val = float_to_str(val, this->oss);
				if (!param.option_axis_x_int_values || str_x_val != str_x_val_prev) {
					cr->move_to(x, inner_y2 + 12);
					cr->show_text(str_x_val);
				}
				if (param.option_axis_x_int_values) str_x_val_prev = str_x_val;
			}
			
			// show axis x unit name
			if (param.axis_x_unit_name.length() > 0) {
				cr->move_to(inner_x2 - (param.axis_x_unit_name.length() + 2) * 5, inner_y2 + 12);
				cr->show_text('(' + param.axis_x_unit_name + ')');
			}
		} else {
			cr->rectangle(inner_x1, inner_y2, alloc_x.length(), Border_Y); cr->fill();
		}
	}
	
	if (param.option_show_axis_y_values) {
		float outer_x1 = param.alloc_outer.get_x();
		if (not_erase)
			oss.precision(get_precision(param.range_y.length() / param.axis_y_divider));
		float val;
		for (unsigned int i = 0; i < axis_y_values.count(); i++) {
			val = axis_y_values[i];
			y = param.range_y.map_reverse(val, alloc_y);
			
			if (! not_erase) {
				cr->rectangle(outer_x1, y - 12, Border_X_Left + 30, 14);
				cr->fill(); continue;
			}
			cr->move_to(outer_x1, y);
			if (i < axis_y_values.count() - 1 || param.axis_y_unit_name.length() == 0)
				cr->show_text(float_to_str(val, this->oss));
			else { // print topmost value with axis y unit name added
				y -= 2; cr->move_to(outer_x1, y);
				cr->show_text(float_to_str(val, this->oss) + '(' + param.axis_y_unit_name + ')');
			}
		}
	}
}

/*------------------------------ PlotParam functions ------------------------------*/

bool PlotParam::reuse_graph(const PlotParam& prev) const
{
	return this->data_cnt           >= prev.data_cnt          //buffer has not been cleared
	    && this->data_cnt_overall   >= prev.data_cnt_overall
	    && this->alloc.get_width()  == prev.alloc.get_width()
	    && this->alloc.get_height() == prev.alloc.get_height()
	    && this->y_av_alloc         == prev.y_av_alloc
	    && this->range_x            == prev.range_x
	    && this->range_y            == prev.range_y
	    && this->index_step         == prev.index_step
	    
	    && this->color_plot         == prev.color_plot
	    && this->option_anti_alias  == prev.option_anti_alias
	    && this->axis_x_divider     == prev.axis_x_divider
	    && this->axis_y_divider     == prev.axis_y_divider
	    
	    && this->option_fixed_scale        == prev.option_fixed_scale
	    && this->option_show_axis_x_values == prev.option_show_axis_x_values
	    && this->option_show_axis_y_values == prev.option_show_axis_y_values
	    && this->option_show_average_line  == prev.option_show_average_line

	    && (   !this->option_show_axis_x_values
	        || (   this->option_axis_x_int_values == prev.option_axis_x_int_values
	            && this->axis_x_unit == prev.axis_x_unit
	            && this->axis_x_unit_name == prev.axis_x_unit_name))
	    && (   !this->option_show_axis_y_values
	        ||  this->axis_y_unit_name == prev.axis_y_unit_name);
}

bool PlotParam::reuse_data(const PlotParam& prev) const
{
	return this->data_cnt >= prev.data_cnt
	    && this->data_cnt_overall >= prev.data_cnt_overall
	    && this->index_step == prev.index_step
		&& this->range_y == prev.range_y
		&& this->alloc.get_height() == prev.alloc.get_height()
		&& intersection(this->range_x, prev.range_x).count() >= prev.index_step;
}

/*------------------------------ PlotBuffer functions ------------------------------*/

PlotBuffer::PlotBuffer() {}

PlotBuffer::PlotBuffer(CircularBuffer* src, unsigned int cnt_limit)
{
	this->init(src);
}

void PlotBuffer::init(CircularBuffer* src, unsigned int cnt_limit)
{
	this->source = src;
	this->buf_cr_cnt_max = cnt_limit;
	
	this->buf_cr_size = 2 * cnt_limit;
	unsigned int buf_cr_spike_size = 4 * src->spike_buffer_size();
	bool except_caught = false;
	try {
		this->buf_spike = new unsigned long int[src->spike_buffer_size()];
		this->buf_cr = new cairo_path_data_t[this->buf_cr_size + buf_cr_spike_size];
	} catch (std::bad_alloc) {
		except_caught = true;
	}
	if (except_caught || this->buf_spike == NULL || this->buf_cr == NULL) {
		if (this->buf_spike) {delete[] this->buf_spike; this->buf_spike = NULL;}
		throw std::bad_alloc();
	}
	
	this->buf_cr_spike = this->buf_cr + this->buf_cr_size;
	
	// initialize the cairo path buffer
	cairo_path_data_t data_head;
	data_head.header.type = CAIRO_PATH_LINE_TO; data_head.header.length = 2;
	for (unsigned int i = 0; i < this->buf_cr_size; i += 2)
		this->buf_cr[i] = data_head;
	
	// initialize the spike segment of the cairo path buffer
	data_head.header.type = CAIRO_PATH_MOVE_TO;
	for (unsigned int i = 0; i < buf_cr_spike_size; i += 4)
		this->buf_cr_spike[i] = data_head;
	data_head.header.type = CAIRO_PATH_LINE_TO;
	for (unsigned int i = 2; i < buf_cr_spike_size; i += 4)
		this->buf_cr_spike[i] = data_head;
}

PlotBuffer::~PlotBuffer()
{
	if (this->buf_spike) delete[] this->buf_spike;
	if (this->buf_cr) delete[] this->buf_cr;
}

bool PlotBuffer::sync(const PlotParam& param, bool forced_sync)
{
	unsigned int step = param.index_step;
	if (! param) return false;
	if (param.range_x.count_by_step(step) > this->buf_cr_cnt_max) return false;
	this->source->lock();
	
	this->flag_redraw = forced_sync || !param.reuse_graph(this->param);
	
	IndexRange range_data = param.data_range_x();
	range_data.step_align_with(this->range_data, step);
	
	IndexRange range_data_l, range_data_r; unsigned int cur_buf_l, cur_buf_r;
	bool flag_reuse_data = true;
	
	// calculate the ranges of new data to be loaded
	if (this->flag_redraw || range_data.max() > this->range_data.max()) {
		if (param.alloc_x_step() != this->param.alloc_x_step())
			this->buf_cr_refresh_x(param.alloc_x_step());
		
		// check if y-axis data can be reused
		if (!forced_sync && param.reuse_data(this->param)) {
			if (range_data.min() < this->range_data.min()) {
				range_data_l.set(range_data.min(), this->range_data.min() - 1);
				cur_buf_l = this->cur_move
					(this->cur_buf_cr, -(long int)range_data_l.count_by_step(step));
			}
			range_data_r.set(this->range_data.max() + 1, range_data.max());
			if (range_data_r) cur_buf_r = this->cur_move(this->cur_buf_cr, this->cnt_buf_cr);
		} else {
			flag_reuse_data = false;
			cur_buf_l = 0; range_data_l = range_data;
		}
	}
	
	this->param = param;
	if (param.index_step > 1 && (flag_redraw || range_data_r))
		this->buf_cr_spike_sync();
	
	this->source->unlock();
	
	if (range_data_l) this->buf_cr_load(cur_buf_l, range_data_l);
	if (range_data_r) this->buf_cr_load(cur_buf_r, range_data_r);
	
	if (flag_reuse_data)
		this->cur_buf_cr = this->cur_move(this->cur_buf_cr,
			subtract(range_data.min(), this->range_data.min()) / (int)this->param.index_step);
	else
		this->cur_buf_cr = 0;
	this->cnt_buf_cr = range_data.count_by_step(this->param.index_step);
	
	if (! this->flag_redraw) {
		this->cur_ext = cur_buf_r;
		this->cnt_ext = range_data_r.count_by_step(this->param.index_step); //0 if range_data_r is empty
	}
	
	this->range_data = range_data;
	return true;
}

void PlotBuffer::buf_cr_refresh_x(float x_step)
{
	if (x_step == 0 || x_step == this->buf_cr_x_step) return;
	
	float x = 0;
	for (unsigned int i = 0, i_buf = 1; i < buf_cr_cnt_max; i++, i_buf += 2) {
		this->buf_cr[i_buf].point.x = x; x += x_step;
	}
	this->buf_cr_x_step = x_step;
}

void PlotBuffer::buf_cr_load(unsigned int cur_buf_cr, IndexRange range_data)
{
	AxisRange alloc_y = this->param.alloc_y();
	this->i_buf_cr = this->cur_to_i(cur_buf_cr);
	for (unsigned int i = range_data.min(); i <= range_data.max(); i += this->param.index_step)
		this->buf_cr_add(this->param.range_y.map_reverse(this->source->abs_index_item(i), alloc_y));
}

void PlotBuffer::buf_cr_spike_sync()
{
	unsigned int cnt_sp = this->source->get_spikes
		(this->source->range_to_rel(this->param.range_x), this->buf_spike);
	
	this->i_buf_cr_spike = 1; //clears buf_cr_spike
	if (cnt_sp < 2) return;
	
	AxisRange alloc_x = this->param.alloc_x(), alloc_y = this->param.alloc_y();
	float x_step = alloc_x.length() / this->param.range_x.length();
	
	unsigned long int i; float x, y;
	for (unsigned int i_sp = 0; i_sp < cnt_sp - 2; i_sp++) {
		i = this->buf_spike[i_sp];
		x = this->param.range_x.map(i, alloc_x);
		y = this->param.range_y.map_reverse(this->source->abs_index_item(i), alloc_y);
		
		// "spikes" are actually turning points, don't draw if it wouldn't turn back soon
		if (this->buf_spike[i_sp + 2] >= i + 2*this->param.index_step) continue;
		this->buf_cr_spike_add(x, y);
		
		x += x_step;
		y = this->param.range_y.map_reverse(this->source->abs_index_item(i + 1), alloc_y);
		this->buf_cr_spike_add(x, y);
	}
}

static inline void cr_append(const Cairo::RefPtr<Cairo::Context>& cr, cairo_path_data_t* data, int num_data)
{
	if (num_data == 0) return;
	cairo_path_t path_info = {CAIRO_STATUS_SUCCESS, data, num_data};
	
	bool flag_recover = false;
	if (data->header.type == CAIRO_PATH_LINE_TO) {
		data->header.type = CAIRO_PATH_MOVE_TO;
		flag_recover = true;
	}
	
	cairo_append_path(cr->cobj(), &path_info);
	
	if (flag_recover)
		data->header.type = CAIRO_PATH_LINE_TO;
}

void PlotBuffer::cairo_load(const Cairo::RefPtr<Cairo::Context>& cr, bool forced_redraw)
{
	if (! this->cnt_buf_cr) return;
	if (forced_redraw) this->flag_redraw = true;
	if (!this->flag_redraw && !this->cnt_ext) return;
	
	unsigned int cur, cnt; float x_cur = this->param.alloc.get_x();
	if (this->flag_redraw) {
		cur = this->cur_buf_cr; cnt = this->cnt_buf_cr;
	} else {
		cur = this->cur_move(this->cur_ext, -1); cnt = this->cnt_ext + 1; //start from the end of previous segment
		x_cur += (this->cnt_buf_cr - cnt)*this->param.alloc_x_step();
	}
	BufRangeMap map(IndexRange(0, cnt - 1), this->buf_cr_cnt_max, cur);
	
	Cairo::Matrix matrix_org = cr->get_matrix(); //this is useful if cr is provided by on_draw()
	cr->translate(-(float)map.former.min()*this->param.alloc_x_step() + x_cur, 0);
	cr_append(cr, this->buf_cr + this->cur_to_i(map.former.min()) - 1, 2*map.former.count());
	
	if (map.latter) {
		cr->set_matrix(matrix_org);
		cr->translate(x_cur + map.former.count()*this->param.alloc_x_step(), 0);
		cr_append(cr, this->buf_cr + this->cur_to_i(map.latter.min()) - 1, 2*map.latter.count());
	}
	
	cr->set_matrix(matrix_org);
	if (this->param.index_step > 1)
		cr_append(cr, this->buf_cr_spike, this->i_buf_cr_spike - 1);
	
	flag_redraw = false;
}

