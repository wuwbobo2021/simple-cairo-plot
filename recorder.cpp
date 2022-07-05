// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include "recorder.h"

#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>

#include <glibmm/timer.h>
#include <gtkmm/adjustment.h>

using namespace SimpleCairoPlot;

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
	this->ptrs = new VariableAccessPtr[var_cnt];
	this->bufs = new CircularBuffer[var_cnt];
	this->areas = new PlottingArea[var_cnt];
	this->eventboxes = new Gtk::EventBox[var_cnt];
	
	Gtk::Label* label_var_bar_ind = Gtk::manage(new Gtk::Label("Variables:"));
	this->box_var_names.pack_start(*label_var_bar_ind, Gtk::PACK_SHRINK);
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		this->ptrs[i] = ptrs[i];
		if (this->ptrs[i].name_csv == "") this->ptrs[i].name_csv = "var" + std::to_string(i + 1);
		if (this->ptrs[i].name_friendly == "") this->ptrs[i].name_friendly = this->ptrs[i].name_csv;
		Gtk::Label* label_name = Gtk::manage(new Gtk::Label(this->ptrs[i].name_friendly));
		label_name->override_color(this->ptrs[i].color_plot);
		this->box_var_names.pack_start(*label_name, Gtk::PACK_SHRINK);
		
		this->bufs[i].init(buf_size);
		this->areas[i].init(& this->bufs[i]);
		this->areas[i].color_plot = this->ptrs[i].color_plot;
		this->eventboxes[i].add(this->areas[i]);
		this->pack_start(this->eventboxes[i], Gtk::PACK_EXPAND_WIDGET);
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
		this->thread_timer = new std::thread(&Recorder::record_loop, this);
	} catch (std::exception ex) {
		this->flag_recording = false;
		this->thread_timer = NULL;
		return false;
	}
	
	return true;
}

void Recorder::stop()
{
	if (! this->var_cnt) return;
	if (! this->flag_recording) return;
	this->flag_recording = false;
	
	if (this->thread_timer) {
		this->thread_timer->join();
		delete this->thread_timer;
	}
}

bool Recorder::open_csv(std::string& file_path)
{	
	std::ifstream ifs(file_path, std::ios_base::in);
	if (! ifs.is_open()) return false;

	if (this->flag_recording) this->stop();
	
	const unsigned int Line_Length_Max = 4096;
	char str[Line_Length_Max] = "\0";
	
	unsigned int var_cnt_csv = 1;
	std::istringstream ist; bool head = true, suc = true; float cur_val;
	while (ifs && !ifs.eof()) {
		ifs.getline(str, Line_Length_Max);
		if (str[0] == '\0' || str[0] == '\r' || str[0] == '\n') continue; //empty line
		
		for (unsigned int i = 0; i < Line_Length_Max; i++) {
			if (str[i] == ',') {
				str[i] = ' ';
				if (head) var_cnt_csv++;
			} else
				if (str[i] == '\0') break;
		}
		if (head) {
			this->clear(); //put here to avoid clearing when opening an empty file
			if (var_cnt_csv > this->var_cnt) var_cnt_csv = this->var_cnt;
		}
		
		ist.str(str);
		for (unsigned int i = 0; i < var_cnt_csv; i++) {
			ist >> cur_val;
			if (head && i == 0 && !ist) break; //header consists of names, not numeric values
			this->bufs[i].push(cur_val);
		}
		if (!ist) {
			ist.clear(); if (! head) suc = false;
		}
		
		if (head) head = false;
	}
	ifs.close();
	
	if (head) return false; //empty file, head not parsed
	
	this->set_index_range();
	this->scrollbar.get_adjustment()->set_value(0);
	this->refresh_areas(true, true);
	return suc;
}

bool Recorder::save_csv(std::string& file_path)
{
	if (! this->var_cnt) return false;
	if (this->flag_recording) this->stop();
	
	std::ofstream ofs(file_path, std::ios_base::out);
	if (! ofs.is_open()) return false;
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		ofs << this->ptrs[i].name_csv;
		if (i + 1 < this->var_cnt) ofs << ',';
	}
	ofs << "\r\n";
	
	for (unsigned int i = 0; i < this->bufs[0].count(); i++) {
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
	if (new_interval <= 0) return false;
	
	this->interval = new_interval;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_axis_x_unit(new_interval / 1000.0);
	
	return true;
}

bool Recorder::set_index_range(unsigned int range_width)
{
	if (! this->var_cnt) return false;
	if (range_width == 0)
		range_width = this->areas[0].get_range_x().length();
	
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_range_x(AxisRange(0, range_width)); //option_auto_goto_end is enabled by default
	this->scrollbar.get_adjustment()->set_page_size(range_width);
	
	return true;
}

bool Recorder::set_index_unit(float unit)
{
	if (unit <= 0) return false;
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].set_axis_x_unit(unit);
	return true;
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
	using namespace std::chrono;
	using namespace std::this_thread;
	
	steady_clock::time_point time_last_refresh = steady_clock::now();
	unsigned int refresh_interval = this->interval; long int check_time_interval;
	if (refresh_interval < 20) refresh_interval = 20; //maximum graph refresh rate: 50 Hz
	
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
					this->thread_timer->detach(); delete this->thread_timer;
					this->dispatcher_sig_full.emit(); return;
				} else
					this->dispatcher_sig_full.emit();
			}
			this->dispatcher_range_update.emit(); //calls scroll_range_update() in main thread
		}
		
		if (steady_clock::now() >= time_last_refresh + milliseconds(refresh_interval)) {
			// refresh graph view
			time_last_refresh = steady_clock::now();
			this->refresh_areas();
		}
		
		t += microseconds((int)(this->interval * 1000.0));
		if (t <= steady_clock::now()) continue; //rarely happens
		
		// better than this_thread::sleep_until() on Windows
		while (true) {
			check_time_interval = (t - steady_clock::now()).count() / 1000 / 2; //us
			if (check_time_interval <= 5) break;
			Glib::usleep(check_time_interval);
		}
		while (steady_clock::now() < t); //dead loop at last, no more than 10 us
		// note: time cost between each loop cannot be ignored.
	}
}

void Recorder::on_scroll()
{	
	Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
	unsigned int val = adj->get_value();
	bool goto_end = (val >= adj->get_upper() - adj->get_page_size());
	
	for (unsigned int i = 0; i < this->var_cnt; i++) {
		this->areas[i].option_auto_goto_end = goto_end;
		if (goto_end == false || this->flag_on_zoom)
			this->areas[i].set_range_x(AxisRange(val, val + adj->get_page_size()));
	}
	
	if (! this->flag_recording) this->refresh_areas(true);
}

void Recorder::scroll_range_update()
{
	Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
	bool goto_end = (adj->get_value() >= adj->get_upper() - adj->get_page_size());
	adj->set_upper(this->bufs[0].count() - 1);
	if (goto_end)
		adj->set_value(adj->get_upper() - adj->get_page_size());
}

bool Recorder::on_button_press(GdkEventButton *event)
{
	if (this->bufs[0].count() == 0) return true;
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
	} else {
		range_x_new.scale(2, x);
		range_x_new = this->bufs[0].range().fit_in_range(range_x_new);
	}
	
	this->flag_on_zoom = true;
	Glib::RefPtr<Gtk::Adjustment> adj = this->scrollbar.get_adjustment();
	adj->configure(range_x_new.min(), 0, this->bufs[0].count() - 1, 1, 200, range_x_new.length());
	this->flag_on_zoom = false;
	return true;
}

void Recorder::refresh_areas(bool forced_check_range_y, bool forced_adapt)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].refresh(forced_check_range_y, forced_adapt);
}

