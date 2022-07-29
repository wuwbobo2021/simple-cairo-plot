// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include "recorder.h"

#include <cstdlib> //strtof(): convert from string to float, faster than stringstream on Windows
#include <cmath> //fabs()
#include <ctime> //localtime()
#include <iomanip> //put_time()
#include <fstream>
#include <sstream>
#include <glibmm/timer.h>
#include <gtkmm/adjustment.h>

using namespace std::chrono;
using namespace SimpleCairoPlot;

namespace SimpleCairoPlot {
	const std::string Empty_Comment = "";
	const unsigned int Line_Length_Max = 4096;
}

Recorder::Recorder():
	Box(Gtk::ORIENTATION_VERTICAL, 10),
	scrollbox(Gtk::ORIENTATION_HORIZONTAL, PlottingArea::Border_X_Left - 2),
	scrollbar(Gtk::Adjustment::create(0, 0, 200, 1, 200, 200), Gtk::ORIENTATION_HORIZONTAL),
	box_var_names(Gtk::ORIENTATION_HORIZONTAL, 20)
{}

Recorder::Recorder(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size):
	Box(Gtk::ORIENTATION_VERTICAL, 10),
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
	this->ptrs = new VariableAccessPtr[var_cnt];
	this->bufs = new CircularBuffer[var_cnt];
	this->areas = new PlottingArea[var_cnt];
	this->eventboxes = new Gtk::EventBox[var_cnt];
	
	Gtk::Label* label_var_bar_ind = Gtk::manage(new Gtk::Label("Variables:"));
	this->box_var_names.pack_start(*label_var_bar_ind, Gtk::PACK_SHRINK);
	this->box_var_names.pack_end(label_axis_x_unit, Gtk::PACK_SHRINK);
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		this->ptrs[i] = ptrs[i];
		this->bufs[i].init(buf_size);
		this->areas[i].init(& this->bufs[i]);
		this->areas[i].axis_y_unit_name = this->ptrs[i].unit_name;
		this->areas[i].color_plot = this->ptrs[i].color_plot;
		this->eventboxes[i].add(this->areas[i]);
		this->pack_start(this->eventboxes[i], Gtk::PACK_EXPAND_WIDGET);
		
		if (this->ptrs[i].name_csv == "") this->ptrs[i].name_csv = "var" + std::to_string(i + 1);
		if (this->ptrs[i].name_friendly == "") this->ptrs[i].name_friendly = this->ptrs[i].name_csv;
		Gtk::Label* label_name = Gtk::manage(new Gtk::Label(this->ptrs[i].name_friendly));
		if (this->ptrs[i].unit_name == "")
			label_name->set_label(this->ptrs[i].name_friendly);
		else
			label_name->set_label(this->ptrs[i].name_friendly + " (" + this->ptrs[i].unit_name + ')');
		label_name->override_color(this->ptrs[i].color_plot);
		this->box_var_names.pack_start(*label_name, Gtk::PACK_SHRINK);
	}
	
	this->scrollbar.get_adjustment()->configure(0, 0, 200, 1, 200, 200); //uninitialized adjustment
	this->scrollbar.signal_value_changed().connect(sigc::mem_fun(*this, &Recorder::on_scroll));
	this->scrollbox.pack_start(this->space_left_of_scroll, Gtk::PACK_SHRINK);
	this->scrollbox.pack_start(this->scrollbar, Gtk::PACK_EXPAND_WIDGET);
	this->pack_start(this->scrollbox, Gtk::PACK_SHRINK);
	
	this->pack_start(this->box_var_names, Gtk::PACK_SHRINK);
	
	this->dispatcher_range_update.connect(sigc::mem_fun(*this, &Recorder::scroll_range_update));
	this->dispatcher_sig_full.connect(sigc::mem_fun(this->sig_full, &sigc::signal<void()>::emit));
	
	this->set_interval(10);
	this->set_index_range(200);
	
	for (unsigned int i = 0; i < this->var_cnt - 1; i++)
		this->areas[i].option_show_axis_x_values = false;
	
	sigc::slot<bool, GdkEventButton*> slot_press;
	slot_press = sigc::mem_fun(*this, &Recorder::on_button_press);
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->eventboxes[i].signal_button_press_event().connect(slot_press);
}

Recorder::~Recorder()
{
	if (! this->var_cnt) return;
	
	this->stop();
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->remove(eventboxes[i]);
	
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
	
	this->t_start = system_clock::now();
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

bool Recorder::open_csv(const std::string& file_path)
{
	std::ifstream ifs(file_path, std::ios_base::in);
	if (! ifs.is_open()) return false;

	if (this->flag_recording) this->stop();
	
	char str[Line_Length_Max] = "\0";
	unsigned int var_cnt_csv = 1;
	bool flag_head = true, suc = true;
	char* cur_str_num; char* aft; float cur_val;
	
	while (ifs && !ifs.eof()) {
		ifs.getline(str, Line_Length_Max); //'\n' is not stored into str 
		
		// skip comment line or empty line
		if (str[0] == '#' || str[0] == '/' || str[0] == '\r' || str[0] == '\0') continue;
		
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
			this->bufs[i].push(cur_val);
			cur_str_num = aft; if (*aft != '\0') cur_str_num++;
		}
		
		if (flag_head) flag_head = false;
	}
	
	ifs.close();
	if (flag_head) return false; //empty file, header is not parsed
	
	this->scroll_range_update();
	this->refresh_areas(true, true);
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
		    << "#   Interval: " << this->data_interval() << " ms" << "\r\n\r\n";
		
		std::istringstream ist; ist.str(str_comment);
		char str_line[Line_Length_Max] = "\0";
		while (ist && !ist.eof()) {
			ist.getline(str_line, Line_Length_Max);
			if (str_line[0] == '\r' || str_line[0] == '\0') break;
			ofs << "# " << str_line << "\r\n";
		}
		ofs << "\r\n";
	}
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		ofs << this->ptrs[i].name_csv;
		if (i + 1 < this->var_cnt) ofs << ',';
	}
	ofs << "\r\n";
	
	for (unsigned int i = 0; i < this->data_count(); i++) {
		for (unsigned int j = 0; j < this->var_cnt; j++) {
			ofs << this->bufs[j][i];
			if (j + 1 < this->var_cnt) ofs << ',';
		}
		ofs << "\r\n";
	}
	
	ofs.close(); return true;
}

void Recorder::clear()
{
	this->flag_not_full = true;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->bufs[i].clear(true);
}

sigc::signal<void()> Recorder::signal_full()
{
	return this->sig_full;
}

bool Recorder::set_interval(float new_interval)
{
	if (this->flag_recording) return false;
	if (new_interval <= 0) return false;
	
	this->interval = new_interval;
	this->set_index_unit(new_interval / 1000.0);
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

bool Recorder::set_index_range(unsigned int range_width) //side effect: scroll to index 0
{
	if (! this->var_cnt) return false;
	if (range_width > this->bufs[0].size()) return false;
	
	if (range_width == 0)
		range_width = this->areas[0].get_range_x().length();
	else if (range_width == this->bufs[0].size())
		range_width--;
	
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_range_x(AxisRange(0, range_width)); //option_auto_goto_end is enabled by default
	
	this->scrollbar.get_adjustment()->set_page_size(range_width);
	if (!this->flag_recording || !this->flag_goto_end)
		this->scrollbar.get_adjustment()->set_value(0);
	
	return true;
}

inline bool almost_equal(float val1, float val2) {
	return fabs(val1 - val2) <= (val1 + val2) / 2.0 / 1000.0;
}

bool Recorder::set_index_unit(float unit)
{
	if (unit <= 0) return false;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_axis_x_unit(unit);
	
	float axis_x_unit = (this->interval / 1000.0) / unit;
	std::string axis_x_unit_name;
	
	if (almost_equal(axis_x_unit, 0.001 * 0.001))
		axis_x_unit_name = "us";
	else if (almost_equal(axis_x_unit, 0.001))
		axis_x_unit_name = "ms";
	else if (almost_equal(axis_x_unit, 1.0))
		axis_x_unit_name = "s";
	else if (almost_equal(axis_x_unit, 60.0))
		axis_x_unit_name = "min";
	else if (almost_equal(axis_x_unit, 3600.0))
		axis_x_unit_name = "h";
	else {
		std::ostringstream sst; sst.precision(6); sst << axis_x_unit;
		axis_x_unit_name = sst.str() + " s";
	}
	this->areas[this->var_cnt - 1].axis_x_unit_name = axis_x_unit_name;
	this->label_axis_x_unit.set_label("Unit-X: " + axis_x_unit_name);
	
	return true;
}

bool Recorder::set_y_range_length_min(unsigned int index, float length_min)
{
	if (index > this->var_cnt - 1) return false;
	return this->areas[index].set_axis_y_range_length_min(length_min);
}

bool Recorder::set_y_range(unsigned int index, AxisRange range)
{
	if (index > this->var_cnt - 1) return false;
	return this->areas[index].set_range_y(range);
}

bool Recorder::set_axis_divider(unsigned int x_div, unsigned int y_div)
{
	if (x_div == 0 && y_div == 0) return false;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_axis_divider(x_div, y_div);
	return true;
}

void Recorder::set_option_record_until_full(bool set)
{
	this->option_record_until_full = set;
}

void Recorder::set_option_auto_set_range_y(bool set)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_auto_set_range_y = set;
}

void Recorder::set_option_auto_set_zero_bottom(bool set)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_auto_set_zero_bottom = set;
}

void Recorder::set_option_show_axis_x_values(bool set)
{
	if (! this->var_cnt) return;
	this->areas[this->var_cnt - 1].option_auto_set_range_y = set;
}

void Recorder::set_option_show_axis_y_values(bool set)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_show_axis_y_values = set;
}

void Recorder::set_option_anti_alias(bool set)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].option_anti_alias = set;
}

/*------------------------------ private functions ------------------------------*/

void Recorder::record_loop()
{
	long int check_time_interval; //us
	steady_clock::time_point t = steady_clock::now();
	
	while (this->flag_recording) {
		// read and record current values of variables
		for (unsigned int i = 0; i < this->var_cnt; i++)
			this->bufs[i].push(this->ptrs[i].read());
		
		if (this->flag_not_full) {
			// update scroll range
			if (this->bufs[0].is_full()) {
				this->flag_not_full = false;
				if (this->option_record_until_full) {
					this->flag_recording = false;
					this->thread_refresh->join(); delete this->thread_refresh;
					this->thread_record->detach(); delete this->thread_record;
					this->thread_record = this->thread_refresh = NULL;
					this->dispatcher_sig_full.emit(); return;
				} else
					this->dispatcher_sig_full.emit();
			}
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
	
	steady_clock::time_point t_last_refresh = steady_clock::now();
	
	while (this->flag_recording) {
		if (this->flag_not_full)
			this->dispatcher_range_update.emit(); //calls scroll_range_update() in main thread
		
		if (steady_clock::now() >= t_last_refresh + milliseconds(this->redraw_interval)) {
			// refresh graph view
			t_last_refresh = steady_clock::now();
			this->refresh_areas();
		}
		
		Glib::usleep(check_time_interval);
	}
}

void Recorder::scroll_range_update()
{
	Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
	unsigned int pre_adj_right = adj->get_value() + adj->get_page_size(),
				 new_upper = this->data_count() - 1;
	
	this->flag_goto_end = (pre_adj_right >= adj->get_upper() || pre_adj_right >= new_upper);
	adj->set_upper(this->data_count() - 1);
	if (this->flag_goto_end)
		adj->set_value(adj->get_upper() - adj->get_page_size());
}

void Recorder::on_scroll()
{	
	Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
	unsigned int val = adj->get_value();
	this->flag_goto_end = (val >= adj->get_upper() - adj->get_page_size());
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		this->areas[i].option_auto_goto_end = this->flag_goto_end;
		if (this->flag_goto_end == false || this->flag_on_zoom)
			this->areas[i].set_range_x(AxisRange(val, val + adj->get_page_size()));
	}
	
	if (!this->flag_recording || this->interval >= 100)
		this->refresh_areas(true);
}

bool Recorder::on_button_press(GdkEventButton *event)
{
	if (this->data_count() == 0) return true;
	if (event->type != GDK_BUTTON_PRESS) return true;
	if (event->button != 1 && event->button != 3) return true;
	
	bool zoom_in = (event->button == 1); //is left button?
	
	Gtk::Allocation alloc = this->areas[0].get_allocation();
	AxisRange range_x = this->areas[0].get_range_x(), range_x_new = range_x,
	          range_scr_x = AxisRange(PlottingArea::Border_X_Left, alloc.get_width());
	
	unsigned int x = range_scr_x.map(event->x, range_x);
	
	if (zoom_in) {
		range_x_new.scale(0.5, x);
		range_x_new.set_int();
		if (range_x_new.length() < 2) return true;
	} else
		range_x_new.scale(2, x);
	
	range_x_new.fit_by_range(this->bufs[0].range());
	
	this->flag_on_zoom = true;
	Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
	adj->configure(range_x_new.min(), 0, this->data_count() - 1, 1, 200, range_x_new.length());
	this->on_scroll(); //strange: Gtk::Adjustment::configure() doesn't emit signal_value_changed
	this->flag_on_zoom = false;
	return true;
}

void Recorder::refresh_areas(bool forced_check_range_y, bool forced_adapt)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].refresh(forced_check_range_y, forced_adapt);
}

