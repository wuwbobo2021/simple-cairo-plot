// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#include <simple-cairo-plot/recorder.h>

#include <cstdlib> //strtof(): convert from string to float, faster than stringstream on Windows
#include <cmath> //fabs(), pow()
#include <ctime> //localtime()
#include <iomanip> //put_time()
#include <fstream>
#include <sstream>
#include <glibmm/timer.h>
#include <gtkmm/separator.h>

using namespace std::chrono;
using namespace SimpleCairoPlot;

namespace SimpleCairoPlot {
	const std::string Empty_Comment = "";
	const unsigned int Line_Length_Max = 4096;
}

Recorder::Recorder():
	Box(Gtk::ORIENTATION_VERTICAL, 5),
	scrollbox(Gtk::ORIENTATION_HORIZONTAL, PlottingArea::Border_X_Left - 2),
	scrollbar(Gtk::Adjustment::create(0, 0, 200, 1, 200, 200), Gtk::ORIENTATION_HORIZONTAL),
	box_var_names(Gtk::ORIENTATION_HORIZONTAL, 20)
{}

Recorder::Recorder(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size):
	Box(Gtk::ORIENTATION_VERTICAL, 5),
	scrollbox(Gtk::ORIENTATION_HORIZONTAL, PlottingArea::Border_X_Left - 2),
	scrollbar(Gtk::Adjustment::create(0, 0, 200, 1, 200, 200), Gtk::ORIENTATION_HORIZONTAL),
	box_var_names(Gtk::ORIENTATION_HORIZONTAL, 20)
{
	this->init(ptrs, buf_size);
}

void Recorder::init(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size)
{
	if (this->var_cnt) return;
	
	if (buf_size < 2)
		throw std::invalid_argument("Recorder::init(): invalid buffer size setting (too small).");
	if (ptrs.size() == 0)
		throw std::invalid_argument("Recorder::init(): requires at least 1 VariableAccessPtr.");
	
	this->var_cnt = ptrs.size();
	this->flag_spike_check = (buf_size > Plot_Data_Amount_Limit_Min);
	
	bool except_caught = false;
	try {
		this->ptrs = new VariableAccessPtr[var_cnt];
		this->bufs = new CircularBuffer[var_cnt];
		this->areas = new PlottingArea[var_cnt];
		this->eventboxes = new Gtk::EventBox[var_cnt];
		this->var_labels = new Gtk::Label[var_cnt];
		
		for (unsigned int i = 0; i < this->var_cnt; i++) {
			this->ptrs[i] = ptrs[i];
			this->bufs[i].init(buf_size);
			this->areas[i].init(& this->bufs[i]);
		}
	} catch (std::bad_alloc) {
		except_caught = true;
	}
	if (except_caught || !this->ptrs || !bufs || !areas || !eventboxes || !var_labels) {
		if (this->ptrs) {delete[] this->ptrs; this->ptrs = NULL;}
		if (this->bufs) {delete[] bufs; bufs = NULL;}
		if (this->areas) {delete[] areas; areas = NULL;}
		if (this->eventboxes) {delete[] eventboxes; eventboxes = NULL;}
		throw std::bad_alloc();
	}
	
	sigc::slot<bool, GdkEventButton*> slot_press = sigc::mem_fun(*this, &Recorder::on_button_press);
	sigc::slot<bool, GdkEventMotion*> slot_motion = sigc::mem_fun(*this, &Recorder::on_motion_notify);
	sigc::slot<bool, GdkEventCrossing*> slot_leave = sigc::mem_fun(*this, &Recorder::on_leave_notify);
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		if (this->flag_spike_check)
			this->bufs[i].set_spike_check_ref_min(100.0 * pow(0.1, this->ptrs[i].precision_csv));

		this->areas[i].axis_y_unit_name = this->ptrs[i].unit_name;
		this->areas[i].color_plot = this->ptrs[i].color_plot;
		
		this->eventboxes[i].add(this->areas[i]);
		this->eventboxes[i].set_events(Gdk::POINTER_MOTION_MASK | Gdk::LEAVE_NOTIFY_MASK);
		this->eventboxes[i].signal_button_press_event().connect(slot_press);
		this->eventboxes[i].signal_motion_notify_event().connect(slot_motion);
		this->eventboxes[i].signal_leave_notify_event().connect(slot_leave);
		this->pack_start(this->eventboxes[i], Gtk::PACK_EXPAND_WIDGET);
		
		if (i < this->var_cnt - 1) {
			this->areas[i].option_show_axis_x_values = false;
			
			Gtk::Separator* separator = Gtk::manage(new Gtk::Separator);
			Gtk::Box* box_separator = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL));
			box_separator->pack_start(*separator, Gtk::PACK_SHRINK);
			separator->set_size_request(PlottingArea::Border_X_Left, 2);
			this->pack_start(*box_separator, Gtk::PACK_SHRINK);
		}	
		
		if (this->ptrs[i].name_csv == "") this->ptrs[i].name_csv = "var" + std::to_string(i + 1);
		if (this->ptrs[i].name_friendly == "") this->ptrs[i].name_friendly = this->ptrs[i].name_csv;
		
		Gdk::RGBA color = this->ptrs[i].color_plot;
		Pango::Attribute attr_color = Pango::Attribute::create_attr_foreground
			(color.get_red_u(), color.get_green_u(), color.get_blue_u());
		Pango::AttrList attrs; attrs.insert(attr_color);
		this->var_labels[i].set_attributes(attrs);
		this->box_var_names.pack_start(this->var_labels[i], Gtk::PACK_SHRINK);
	}
	
	this->scrollbar.signal_value_changed().connect(sigc::mem_fun(*this, &Recorder::on_scroll));
	this->scrollbox.pack_start(this->space_left_of_scroll, Gtk::PACK_SHRINK);
	this->scrollbox.pack_start(this->scrollbar, Gtk::PACK_EXPAND_WIDGET);
	this->pack_start(this->scrollbox, Gtk::PACK_SHRINK);
	
	this->oss.setf(std::ios::fixed);
	this->refresh_var_labels();
	this->box_var_names.pack_start(this->label_cursor_x, Gtk::PACK_SHRINK);
	this->box_var_names.pack_end(this->label_axis_x_unit, Gtk::PACK_SHRINK);
	this->pack_start(this->box_var_names, Gtk::PACK_SHRINK);
	
	this->dispatcher_refresh_indicators.connect(sigc::mem_fun(*this, &Recorder::refresh_indicators));
	this->dispatcher_sig_full.connect(sigc::mem_fun(this->sig_full, &sigc::signal<void()>::emit));
	
	this->set_interval(10);
	this->set_axis_x_range(200 - 1);
	this->auto_set_scroll_mode(); //actually turns off goto-end mode, because it's not recording
}

Recorder::~Recorder()
{
	if (! this->var_cnt) return;
	
	this->stop();
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->remove(eventboxes[i]);
	
	delete[] this->var_labels;
	delete[] this->eventboxes;
	delete[] this->areas;
	delete[] this->bufs;
	delete[] this->ptrs;
	this->var_cnt = 0;
}

bool Recorder::start()
{
	if (! this->var_cnt) return false; //not initialized
	if (this->flag_recording) return true;
	
	this->clear();
	
	try {
		this->flag_recording = true;
		this->thread_record = new std::thread(&Recorder::record_loop, this);
		this->thread_refresh = new std::thread(&Recorder::refresh_loop, this);
	} catch (std::exception ex) {
		this->flag_recording = false;
		if (this->thread_record) delete this->thread_record;
		this->thread_record = this->thread_refresh = NULL;
		return false;
	}
	
	return true;
}

void Recorder::stop()
{
	if (! this->var_cnt) return;
	if (! this->flag_recording) return;
	this->flag_recording = false;
	
	if (this->thread_record) {
		this->thread_record->join(); this->thread_refresh->join();
		delete this->thread_record; this->thread_record = NULL;
		delete this->thread_refresh; this->thread_refresh = NULL;
	}
}

void Recorder::clear()
{
	this->flag_full = false;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->bufs[i].clear(true);
}

sigc::signal<void()> Recorder::signal_full()
{
	return this->sig_full;
}

bool Recorder::open_csv(const std::string& file_path)
{
	if (! this->var_cnt) return false;
	
	std::ifstream ifs(file_path, std::ios_base::in);
	if (! ifs.is_open()) return false;

	if (this->flag_recording) this->stop();
	
	char str[Line_Length_Max] = "\0";
	unsigned int var_cnt_csv = 1;
	bool flag_head = true, suc = true;
	char* cur_str_num; char* aft; float cur_val;
	
	while (ifs && !ifs.eof()) {
		ifs.getline(str, Line_Length_Max); //'\n' is not stored into str 
		
		if (str[0] == '#' || str[0] == ';' || str[0] == '/' || str[0] == '\r' || str[0] == '\0')
			continue; // skip comment line or empty line
		
		if (flag_head) { //header of the csv file
			this->clear(); //put here to avoid clearing when opening an empty file
			
			for (unsigned int i = 0; i < Line_Length_Max; i++) {
				if (str[i] == '\0') break;
				if (str[i] == ',') var_cnt_csv++;
			}
			if (var_cnt_csv > this->var_cnt) var_cnt_csv = this->var_cnt;
		}
		
		cur_str_num = str;
		for (unsigned int i = 0; i < var_cnt_csv; i++) {
			if (cur_str_num[0] == '\0') suc = false; //not enough values in this line
			cur_val = std::strtof(cur_str_num, &aft); //aft now points to the first character after the number string
			if (cur_val == 0 && *aft != ',' && *aft != '\r' && *aft != '\0') {
				if (! flag_head) suc = false; //non-numeric character in the middle
				if (i == 0) break;
			}
			this->bufs[i].push(cur_val, this->flag_spike_check);
			cur_str_num = aft; if (*aft != '\0') cur_str_num++;
		}
		
		if (flag_head) flag_head = false;
	}
	
	ifs.close();
	if (flag_head) return false; //empty file, header is not parsed
	
	this->set_axis_x_range(this->data_range());
	return suc;
}

inline std::ostream& operator<<(std::ostream& ost, const system_clock::time_point& t)
{
	const std::time_t t_c = system_clock::to_time_t(t);
	return ost << std::put_time(std::localtime(&t_c), "%Y-%m-%d %H:%M:%S");
}

bool Recorder::save_csv(const std::string& file_path, const std::string& str_comment)
{
	if (! this->var_cnt) return false;
	if (this->data_count() == 0) return false;
	if (this->flag_recording) this->stop();
	
	std::ofstream ofs(file_path, std::ios::out);
	if (! ofs.is_open()) return false;
	
	if (str_comment.length() > 0) {
		ofs << "# First Data: " << this->time_first_data()   << "\r\n"
		    << "#  Last Data: " << this->time_last_data()    << "\r\n"
		    << "#   Interval: " << this->data_interval() << " ms" << "\r\n";
		
		std::istringstream ist; ist.str(str_comment);
		char str_line[Line_Length_Max] = "\0";
		while (ist && !ist.eof()) {
			ist.getline(str_line, Line_Length_Max);
			if ((str_line[0] == '\r' || str_line[0] == '\0') && ist.eof()) break;
			ofs << "# " << str_line << "\r\n";
		}
	}
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		ofs << this->ptrs[i].name_csv;
		if (i + 1 < this->var_cnt) ofs << ',';
	}
	ofs << "\r\n";
	
	ofs.setf(std::ios::fixed);
	for (unsigned int i = 0; i < this->data_count(); i++) {
		for (unsigned int j = 0; j < this->var_cnt; j++) {
			ofs.precision(this->ptrs[j].precision_csv);
			ofs << this->bufs[j][i];
			if (j + 1 < this->var_cnt) ofs << ',';
		}
		ofs << "\r\n";
	}
	
	ofs.close(); return true;
}

bool Recorder::set_interval(float new_interval)
{
	if (! this->var_cnt) return false;
	if (this->flag_recording) return false;
	if (new_interval <= 0) return false;
	
	this->interval = new_interval;
	this->set_index_unit(new_interval / 1000.0); //to seconds
	this->set_redraw_interval(new_interval);
	return true;
}

bool Recorder::set_redraw_interval(unsigned new_redraw_interval)
{
	if (new_redraw_interval == 0) return false;
	this->redraw_interval = new_redraw_interval;
	if (this->redraw_interval < 20)
		this->redraw_interval = 20; //maximum graph refresh rate: 50 Hz
	return true;
}

inline bool almost_equal(float val1, float val2) {
	return fabs(val1 - val2) <= (val1 + val2) / 2.0 / 1000.0;
}

bool Recorder::set_index_unit(float unit)
{
	if (! this->var_cnt) return false;
	if (unit <= 0) return false;
	
	this->axis_x_unit = unit;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_axis_x_unit(unit);
	
	float axis_x_time_unit = (this->interval / 1000.0) / unit;
	
	flag_axis_x_unique_unit = false;
	if (almost_equal(axis_x_time_unit, 0.001 * 0.001))
		this->axis_x_unit_name = "us";
	else if (almost_equal(axis_x_time_unit, 0.001))
		this->axis_x_unit_name = "ms";
	else if (almost_equal(axis_x_time_unit, 1.0))
		this->axis_x_unit_name = "s";
	else if (almost_equal(axis_x_time_unit, 60.0))
		this->axis_x_unit_name = "min";
	else if (almost_equal(axis_x_time_unit, 3600.0))
		this->axis_x_unit_name = "h";
	else {
		this->flag_axis_x_unique_unit = true;
		std::ostringstream sst; sst.precision(6); sst << axis_x_time_unit;
		this->axis_x_unit_name = sst.str() + " s";
	}
	
	this->areas[this->var_cnt - 1].axis_x_unit_name
		= (this->flag_axis_x_unique_unit? "" : this->axis_x_unit_name);
	
	this->label_axis_x_unit.set_visible(this->flag_axis_x_unique_unit);
	if (this->flag_axis_x_unique_unit)
		this->label_axis_x_unit.set_label("Unit-X: " + axis_x_unit_name);
	
	return true;
}

bool Recorder::set_axis_x_range(AxisRange range)
{
	if (! this->var_cnt) return false;
	
	range.set_int();
	if (range.length() > this->data_range_max().length()) return false;
	
	if (range.length() == 0)
		range = this->axis_x_range();
	else if (range.length() == this->data_count_max())
		range.set(0, range.max() - 1);
	else
		range.fit_by_range(this->data_range_max());
	
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_range_x(range);
	
	if (this->flag_recording && !this->flag_full)
		this->auto_set_scroll_mode
			(Gtk::Adjustment::create(range.min(), 0, this->data_range().max(), 1, 1, range.length()));
	
	this->refresh_areas(true, true);
	
	this->flag_refresh_scroll = true;
	this->dispatcher_refresh_indicators.emit();
	return true;
}

bool Recorder::set_axis_y_range(unsigned int index, AxisRange range)
{
	if (index > this->var_cnt - 1) return false;
	return this->areas[index].set_range_y(range);
}

bool Recorder::set_axis_y_range_length_min(unsigned int index, float length_min)
{
	if (index > this->var_cnt - 1) return false;
	return this->areas[index].set_axis_y_range_length_min(length_min);
}

bool Recorder::set_axis_divider(unsigned int x_div, unsigned int y_div)
{
	if (x_div == 0 && y_div == 0) return false;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_axis_divider(x_div, y_div);
	return true;
}

void Recorder::set_option_fixed_axis_scale(bool set)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_fixed_scale = set;
}

void Recorder::set_option_stop_on_full(bool set)
{
	this->option_stop_on_full = set;
}

void Recorder::set_option_auto_extend_range_x(bool set)
{
	if (! this->var_cnt) return;
	this->option_extend_index_range = set;
	this->auto_set_scroll_mode();
}

void Recorder::set_option_auto_set_range_y(unsigned int index, bool set)
{
	if (index > this->var_cnt - 1) return;
	this->areas[index].option_auto_set_range_y = set;
		
}

void Recorder::set_option_auto_set_zero_bottom(unsigned int index, bool set)
{
	if (index > this->var_cnt - 1) return;
	this->areas[index].option_auto_set_zero_bottom = set;
}

void Recorder::set_option_show_axis_x_values(bool set)
{
	if (! this->var_cnt) return;
	this->areas[this->var_cnt - 1].option_auto_set_range_y = set;
}

void Recorder::set_option_axis_x_int_values(bool set)
{
	if (! this->var_cnt) return;
	this->areas[this->var_cnt - 1].option_axis_x_int_values = set;
}

void Recorder::set_option_show_axis_y_values(bool set)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_show_axis_y_values = set;
}

void Recorder::set_option_show_average_line(unsigned int index, bool set)
{
	if (index > this->var_cnt - 1) return;
	this->areas[index].option_show_average_line = set;
}

void Recorder::set_option_anti_alias(bool set)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_anti_alias = set;
}

/*------------------------------ private functions ------------------------------*/

void Recorder::record_loop()
{
	this->tp_start = system_clock::now();
	
	long int check_time_interval; //us
	steady_clock::time_point t = steady_clock::now();
	
	while (this->flag_recording) {
		// read and record current values of variables
		for (unsigned int i = 0; i < this->var_cnt; i++)
			this->bufs[i].push(this->ptrs[i].read(), this->flag_spike_check);
		
		if (!this->flag_full && this->bufs[0].is_full()) {
			this->flag_full = true;
			this->dispatcher_refresh_indicators.emit(); //for the last time
			if (this->option_stop_on_full) {
				this->flag_recording = false;
				this->thread_refresh->join(); delete this->thread_refresh;
				this->thread_record->detach(); delete this->thread_record;
				this->thread_record = this->thread_refresh = NULL;
				this->dispatcher_sig_full.emit(); return;
			} else
				this->dispatcher_sig_full.emit();
		}
		
		t += microseconds((int)(this->interval * 1000.0));
		if (t <= steady_clock::now()) continue; //rarely happens
		
		// better than this_thread::sleep_until() on Windows
		while (true) {
			check_time_interval = (t - steady_clock::now()).count() / 1000 / 2; //us
			if (check_time_interval <= 5) break;
			Glib::usleep(check_time_interval);
		}
		while (steady_clock::now() < t); //empty loop at last, no more than 10 us
		// note: time cost between each loop cannot be ignored.
	}
}

void Recorder::refresh_loop()
{
	long int check_time_interval = this->redraw_interval;
	if (this->redraw_interval > 200) check_time_interval = 200;
	check_time_interval *= 1000; //us
	
	this->refresh_view();
	steady_clock::time_point t_last_refresh = steady_clock::now();
	
	while (this->flag_recording) {		
		if (steady_clock::now() >= t_last_refresh + milliseconds(this->redraw_interval)) {
			// refresh graph view
			t_last_refresh = steady_clock::now();
			this->refresh_areas();
		}
		
		if (!this->flag_full || this->flag_cursor)
			this->dispatcher_refresh_indicators.emit(); //calls refresh_indicators() in main thread
		
		Glib::usleep(check_time_interval);
	}
	
	this->auto_set_scroll_mode(); //actually turns off goto-end mode
}

void Recorder::refresh_indicators() //not thread-safe
{
	if (this->flag_cursor) this->refresh_var_labels();
	if (this->flag_full && !this->flag_refresh_scroll) return;
	
	Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
	AxisRange range_x = this->axis_x_range();
	unsigned int val, upper = this->data_range().max();
	
	if (this->flag_goto_end && !flag_extend && range_x.max() < upper)
		val = upper - range_x.length(); //false update, not limited by redraw_interval
	else
		val = range_x.min();
	
	this->flag_refresh_scroll = true;
	adj->configure(val, 0, upper, 1, range_x.length() / 2, range_x.length());
	this->flag_refresh_scroll = false;
	this->scrollbar.set_visible(val > 0 || adj->get_page_size() < adj->get_upper());
}

void Recorder::on_scroll() //on scrollbar.signal_value_changed()
{
	if (this->flag_refresh_scroll) return;
	
	if (! this->auto_set_scroll_mode()) {
		Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
		unsigned int val = adj->get_value();
		for (unsigned int i = 0; i < this->var_cnt; i++)
			this->areas[i].set_range_x(AxisRange(val, val + adj->get_page_size()));
	}
	
	if (!this->flag_recording || this->interval >= 100)
		this->refresh_areas(true);
}

bool Recorder::on_button_press(GdkEventButton* event)
{
	if (this->data_count() == 0) return true;
	if (event->type != GDK_BUTTON_PRESS) return true;
	if (event->button != 1 && event->button != 3) return true;
	
	bool zoom_in = (event->button == 1); //is left button?
	if (!zoom_in && this->flag_extend) return true;
	
	AxisRange range_scr_x(PlottingArea::Border_X_Left,
	                      this->areas[0].get_allocation().get_width());
	
	AxisRange range_x = this->axis_x_range();
	unsigned int x = range_scr_x.map(event->x, range_x);
	
	if (zoom_in) {
		range_x.scale(0.5, x); range_x.set_int();
		if (range_x.length() < 2) return true;
	} else
		range_x.scale(2, x);
	
	if (range_x.length() <= this->data_range().max())
		range_x.fit_by_range(this->data_range());
	else {
		range_x.fit_by_range(this->data_range_max());
		if (zoom_in && range_x.max() > this->data_range().max())
			range_x.min_move_to(0);
	}
	
	this->set_axis_x_range(range_x);
	return true;
}

bool Recorder::auto_set_scroll_mode(Glib::RefPtr<Gtk::Adjustment> adj) //default param is that of the scrollbar
{
	if (!this->flag_recording || this->flag_full)
		this->flag_goto_end = false;
	else
		this->flag_goto_end = (adj->get_value() + adj->get_page_size() >= adj->get_upper());
	
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_auto_goto_end = this->flag_goto_end;
	
	if (this->flag_goto_end) {
		bool wide_enough = (adj->get_page_size() >= adj->get_upper());
		
		this->flag_extend = this->option_extend_index_range && wide_enough;
		for (unsigned int i = 0; i < this->var_cnt; i++)
			this->areas[i].option_auto_extend_range_x = this->flag_extend;
	} else
		this->flag_extend = false;
	
	return this->flag_goto_end;
}

inline std::string float_to_str(float val, std::ostringstream& oss)
{
	oss.str(""); oss << val;
	return oss.str();
}

bool Recorder::on_motion_notify(GdkEventMotion* motion_event)
{
	this->flag_cursor = (this->cursor_x >= PlottingArea::Border_X_Left);
	this->cursor_x = motion_event->x;
	this->refresh_var_labels();
	return true;
}

bool Recorder::on_leave_notify(GdkEventCrossing* crossing_event)
{
	this->flag_cursor = false;
	this->refresh_var_labels();
	return true;
}

void Recorder::refresh_var_labels()
{
	float x; bool show_values = false;
	if (this->flag_cursor) {
		AxisRange range_scr_x(PlottingArea::Border_X_Left,
		                      this->areas[0].get_allocation().get_width());
		x = range_scr_x.map(this->cursor_x, this->axis_x_range());
		show_values = this->data_range().contain(x);
	}
	
	if (show_values) {
		Glib::ustring str_label;
		
		for (unsigned int i = 0; i < this->var_cnt; i++) {
			oss.precision(this->ptrs[i].precision_csv);
			str_label = this->ptrs[i].name_friendly + ": "
			          + float_to_str(this->bufs[i][x], oss);
			if (this->ptrs[i].unit_name.length() > 0)
				str_label += ' ' + this->ptrs[i].unit_name;
			this->var_labels[i].set_label(str_label);
		}
		
		oss.precision(2);
		str_label = float_to_str(this->t_data(x), oss);
		if (! this->flag_axis_x_unique_unit)
			str_label += ' ' + this->axis_x_unit_name;
		this->label_cursor_x.set_label('(' + str_label + ')');
	}
	else {
		this->label_cursor_x.set_label("");
		
		Glib::ustring var_name;
		for (unsigned int i = 0; i < this->var_cnt; i++) {
			var_name = this->ptrs[i].name_friendly;
			if (this->ptrs[i].unit_name.length() > 0)
				var_name += " (" + this->ptrs[i].unit_name + ')';
			this->var_labels[i].set_label(var_name);
		}
	}
}

