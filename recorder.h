// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_RECORDER_H
#define SIMPLE_CAIRO_PLOT_RECORDER_H

#include <vector>
#include <chrono>
#include <thread>

#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/label.h>

#include "plottingarea.h"

namespace SimpleCairoPlot
{

typedef float (*VariableAccessFuncPtr)(void*);

// used by the recorder to access current values of variables
class VariableAccessPtr
{
	bool is_func_ptr = false;
	
	// used if it's a data pointer
	const float* addr_data = NULL;
	
	// used if it's a function pointer
	VariableAccessFuncPtr addr_func = NULL;
	void* addr_obj = NULL;

public:
	std::string unit_name = "", name_csv = "";
	Glib::ustring name_friendly = "";
	
	unsigned int precision_csv = 3; //decimal digits (in .csv file)
	
	Gdk::RGBA color_plot;
	
	VariableAccessPtr();
	VariableAccessPtr(const float* pd);
	VariableAccessPtr(void* pobj, VariableAccessFuncPtr pfunc);
	void set(const float* pd);
	void set(void* pobj, VariableAccessFuncPtr pfunc);
	float read() const;
};

// this function is regenerated at different address for each class at compile time,
// therefore, it avoids casting from member function pointer to function pointer.
// reference: <https://stackoverflow.com/questions/57198709>.
template <typename T, float (T::*F)()>
float MemberFuncCall(void* pobj)
{
	return (static_cast<T*>(pobj)->*F)();
}

// use this function to create the pointer for a member function.
template <typename T, float (T::*F)()>
inline VariableAccessPtr MemberFuncPtr(T* pobj)
{
	return VariableAccessPtr(static_cast<void*>(pobj), &MemberFuncCall<T, F>);
}

inline VariableAccessPtr::VariableAccessPtr() {}

inline VariableAccessPtr::VariableAccessPtr(const float* pd)
{
	this->set(pd);
}

inline VariableAccessPtr::VariableAccessPtr(void* pobj, VariableAccessFuncPtr pfunc)
{
	this->set(pobj, pfunc);
}

inline void VariableAccessPtr::set(const float* pd)
{
	this->addr_data = pd;
	this->read(); //produce segment fault earlier if the pointer is null
	this->color_plot.set_rgba(1.0, 0.0, 0.0); //red
}

inline void VariableAccessPtr::set(void* pobj, VariableAccessFuncPtr pfunc)
{
	this->is_func_ptr = true;
	this->addr_obj = pobj; this->addr_func = pfunc;
	this->read();
	this->color_plot.set_rgba(1.0, 0.0, 0.0);
}

inline float VariableAccessPtr::read() const
{
	if (this->is_func_ptr)
		return this->addr_func(this->addr_obj);
	else
		return *(this->addr_data);
}

extern const std::string Empty_Comment;

class Recorder: public Gtk::Box
{
	unsigned int var_cnt = 0;
	
	VariableAccessPtr* ptrs = NULL;
	CircularBuffer* bufs = NULL;
	PlottingArea* areas = NULL; Gtk::EventBox* eventboxes = NULL;
	
	Gtk::Scrollbar scrollbar; Gtk::Label space_left_of_scroll; Gtk::Box scrollbox;
	Gtk::Box box_var_names; Gtk::Label label_axis_x_unit;
	Glib::Dispatcher dispatcher_refresh_scroll; volatile bool flag_refresh_scroll = false;
	
	std::thread* thread_record = NULL,
	           * thread_refresh = NULL;
	bool flag_recording = false;
	bool option_record_until_full = false;
	float interval = 10; unsigned int redraw_interval = 20; //in milliseconds
	std::chrono::system_clock::time_point t_start;
	
	bool option_extend_index_range = false;
	volatile bool flag_goto_end = false, flag_extend = false;
	
	volatile bool flag_not_full = true;
	sigc::signal<void()> sig_full;
	Glib::Dispatcher dispatcher_sig_full;
	
	void record_loop();
	void refresh_loop();
	void on_scroll();
	bool on_button_press(GdkEventButton *event);
	void refresh_scroll();
	bool auto_set_scroll_mode();
	bool auto_set_scroll_mode(Glib::RefPtr<Gtk::Adjustment> adj);
	void refresh_areas(bool forced_check_range_y = false, bool forced_adapt = false);
	
public:
	Recorder(); void init(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size);
	Recorder(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size);
	virtual ~Recorder();
	
	bool is_recording() const;
	float data_interval() const;
	unsigned int var_count() const;
	unsigned int data_count() const;
	std::chrono::system_clock::time_point time_start() const;
	std::chrono::system_clock::time_point time_first_data() const;
	std::chrono::system_clock::time_point time_last_data() const;
	
	bool start(); //start recording
	void stop();
	CircularBuffer& buffer(unsigned int index) const; //direct access to each data buffer
	bool open_csv(const std::string& file_path); //note: comments will not be loaded
	bool save_csv(const std::string& file_path, const std::string& str_comment = Empty_Comment); //note: comment is unstandard
	void clear();
	
	sigc::signal<void()> signal_full();
	
	bool set_interval(float new_interval); //interval of reading current values, in milliseconds
	bool set_redraw_interval(unsigned int new_redraw_interval); //set manually if a slower redraw rate is required to reduce CPU usage
	
	bool set_index_range(AxisRange range); //range.width() + 1 is the amount of data shows in each area
	bool set_index_range(unsigned int range_width = 0); //equal to AxisRange(0, range_width)
	bool set_index_unit(float unit); //it should be the data interval, index values are multiplied by the unit
	bool set_y_range_length_min(unsigned int index, float length_min); //minimum range length of y-axis range in auto-set mode
	bool set_y_range(unsigned int index, AxisRange range); //useless when option_auto_set_range_y is set
	bool set_axis_divider(unsigned int x_div, unsigned int y_div); //how many segments the axis should be divided into
	
	void set_option_record_until_full(bool set); //stop after the buffers become full, default: false
	void set_option_auto_extend_index_range(bool set); //extend index range to show all existing data. default: false
	void set_option_auto_set_range_y(unsigned int index, bool set); //if not, the user must set the range for each area. default: true
	void set_option_auto_set_zero_bottom(unsigned int index, bool set); //set the bottom of range y to zero in auto mode. default: true
	void set_option_show_axis_x_values(bool set); //only that of the bottommost is shown. default: true
	void set_option_axis_x_int_values(bool set); //don't show decimal digits for x-axis values. default: false
	void set_option_show_axis_y_values(bool set); //shown in left border of each area. default: true
	void set_option_anti_alias(bool set); //font of x-axis, y-axis values are not influenced. default: false
};

inline bool Recorder::is_recording() const
{
	return this->flag_recording;
}

inline float Recorder::data_interval() const
{
	return this->interval;
}

inline unsigned int Recorder::var_count() const
{
	return this->var_cnt;
}

inline unsigned int Recorder::data_count() const
{
	return this->bufs[0].count();
}

inline std::chrono::system_clock::time_point Recorder::time_start() const
{
	return this->t_start;
}

inline std::chrono::system_clock::time_point Recorder::time_first_data() const
{
	return this->t_start + std::chrono::microseconds(
		(long int)(this->bufs[0].count_overwriten() * 1000.0 * this->interval)
	);
}

inline std::chrono::system_clock::time_point Recorder::time_last_data() const
{
	return this->t_start + std::chrono::microseconds(
		(long int)((this->bufs[0].count_overwriten() + this->data_count() - 1) * 1000.0 * this->interval)
	);
}

inline CircularBuffer& Recorder::buffer(unsigned int index) const
{
	return this->bufs[index];
}

inline bool Recorder::set_index_range(unsigned int range_width)
{
	return this->set_index_range(AxisRange(0, range_width));
}

// private
inline bool Recorder::auto_set_scroll_mode()
{
	return this->auto_set_scroll_mode(this->scrollbar.get_adjustment());
}

inline void Recorder::refresh_areas(bool forced_check_range_y, bool forced_adapt)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].refresh(forced_check_range_y, forced_adapt);
}

}

#endif

