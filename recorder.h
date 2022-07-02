// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#ifndef SIMPLE_CAIRO_PLOT_RECORDER_H
#define SIMPLE_CAIRO_PLOT_RECORDER_H

#include <vector>
#include <thread>

#include <gtkmm/box.h>
#include <gtkmm/eventbox.h>
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
	Gdk::RGBA color_plot; std::string name_csv = "", name_friendly = "";
	
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

class Recorder: public Gtk::Box
{
	unsigned int var_cnt = 0;
	
	VariableAccessPtr* ptrs = NULL;
	CircularBuffer* bufs = NULL;
	PlottingArea* areas = NULL; Gtk::EventBox* eventboxes = NULL;
	
	Gtk::Scrollbar scrollbar; Gtk::Label space_left_of_scroll; Gtk::Box scrollbox; Gtk::Box box_var_names;
	Glib::Dispatcher dispatcher_range_update;
	bool flag_on_zoom = false;
	
	std::thread* thread_timer = NULL;
	bool flag_recording = false;
	bool option_record_until_full = false;
	float interval = 10; //in milliseconds
	
	bool flag_not_full = true;
	sigc::signal<void()> sig_full;
	Glib::Dispatcher dispatcher_sig_full;
	
	void record_loop();
	void on_scroll();
	bool on_button_press(GdkEventButton *event);
	void scroll_range_update();
	void refresh_areas(bool forced_check_range_y = false, bool forced_adapt = false);
	
public:
	Recorder(); void init(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size);
	Recorder(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size);
	virtual ~Recorder();
	
	bool is_recording() const;
	unsigned int var_count() const;
	bool start(); //start recording
	void stop();
	CircularBuffer& buffer(unsigned int index) const; //direct access to each data buffer
	bool open_csv(std::string& file_path);
	bool save_csv(std::string& file_path);
	void clear();
	
	sigc::signal<void()> signal_full();
	
	bool set_interval(float new_interval); //interval of reading current values, in milliseconds
	
	bool set_index_range(unsigned int range_width = 0); //range_width + 1 is the amount of data shows in each area
	bool set_index_unit(float unit); //it should be the data interval, index values are multiplied by the unit
	bool set_y_range(unsigned int index, AxisRange range); //useless when option_auto_set_range_y is set
	bool set_axis_divider(unsigned int x_div, unsigned int y_div); //how many segments the axis should be divided into
	
	void set_option_record_until_full(bool set); //stop after the buffers become full, default: false
	void set_option_auto_set_range_y(bool set); //if it's not set, the user must set the range for each area
	void set_option_auto_set_zero_bottom(bool set); //if the bottom of range y should be zero in auto set mode
	void set_option_show_axis_x_values(bool set); //only that of the bottommost is shown
	void set_option_show_axis_y_values(bool set); //shown in left border of each area
	void set_option_anti_alias(bool set);
};

inline bool Recorder::is_recording() const
{
	return this->flag_recording;
}

inline unsigned int Recorder::var_count() const
{
	return this->var_cnt;
}

inline CircularBuffer& Recorder::buffer(unsigned int index) const
{
	return this->bufs[index];
}

}

#endif

