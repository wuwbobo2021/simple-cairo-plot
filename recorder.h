// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

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

#include <simple-cairo-plot/plotarea.h>

namespace SimpleCairoPlot
{
class Recorder; class VariableAccessPtr;
using RecordView = Recorder;

using VariableAccessFuncPtr = float (*)(void*);

// used by the recorder to access current values of variables
class VariableAccessPtr
{
public:
	std::string unit_name = "", name_csv = "";
	Glib::ustring name_friendly = "";
	
	unsigned int precision_csv = 3; //decimal digits (in .csv file)
	
	Gdk::RGBA color_plot;
	
	VariableAccessPtr();
	VariableAccessPtr(const volatile float* pd);
	VariableAccessPtr(void* pobj, VariableAccessFuncPtr pfunc);
	void set(const volatile float* pd);
	void set(void* pobj, VariableAccessFuncPtr pfunc);
	float read() const;
	
private:
	bool is_func_ptr = false;
	
	// used if it's a data pointer
	const volatile float* addr_data = NULL;
	
	// used if it's a function pointer
	VariableAccessFuncPtr addr_func = NULL;
	void* addr_obj = NULL;
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

inline VariableAccessPtr::VariableAccessPtr(const volatile float* pd)
{
	this->set(pd);
}

inline VariableAccessPtr::VariableAccessPtr(void* pobj, VariableAccessFuncPtr pfunc)
{
	this->set(pobj, pfunc);
}

inline void VariableAccessPtr::set(const volatile float* pd)
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
public:
	// note: some inline functions are NOT safe before initialization
	Recorder(); void init(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size);
	Recorder(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size);
	virtual ~Recorder();
	
	bool is_recording() const;
	float data_interval() const;
	unsigned int var_count() const;
	unsigned int data_count() const; IndexRange data_range() const;
	unsigned int data_count_max() const; IndexRange data_range_max() const;
	
	float t_data(unsigned int i) const; //the unit is determined by set_index_unit() (default: s)
	float t_first_data() const;
	float t_last_data() const;
	std::chrono::system_clock::time_point time_start() const;
	std::chrono::system_clock::time_point time_data(unsigned int i) const;
	std::chrono::system_clock::time_point time_first_data() const;
	std::chrono::system_clock::time_point time_last_data() const;
	
	// direct access to each data buffer. it is possible to push data manually
	// into the buffers, and call set_axis_x_range() at last to show them.
	CircularBuffer& buffer(unsigned int index) const;
	
	IndexRange axis_x_range() const; //index range in the buffers
	ValueRange axis_y_range(unsigned int index) const;
	
	bool start(); //clears the buffers
	void stop();
	void clear();
	
	sigc::signal<void()> signal_full();
	
	bool open_csv(const std::string& file_path); //note: comments will not be loaded
	bool save_csv(const std::string& file_path, const std::string& str_comment = Empty_Comment); //note: comment is unstandard
	
	void refresh_view(); //call this function if new data has been loaded into the buffers manually
	
	bool set_interval(float new_interval); //interval of reading current values (ms). it sets index unit (multipier) to interval (s)
	bool set_redraw_interval(unsigned int new_redraw_interval); //set manually if a slower redraw rate is required to reduce CPU usage
	
	bool set_index_unit(float unit); //note: set to interval in ms, s (default), min or h. index values are multiplied by the unit
	
	bool set_axis_x_range(IndexRange range); //range.width() + 1 is the amount of data shows in each area
	bool set_axis_x_range(unsigned int range_width = 0); //equal to IndexRange(0, range_width) except in goto-end mode
	bool set_axis_x_range(unsigned int min, unsigned int max); //equal to IndexRange(min, max)
	bool set_axis_y_range(unsigned int index, ValueRange range); //useless when option_auto_set_range_y is set
	bool set_axis_y_range_length_min(unsigned int index, float length_min); //minimum range length of y-axis range in auto-set mode
	
	bool set_axis_divider(unsigned int x_div, unsigned int y_div); //how many segments the axis should be divided into
	void set_option_fixed_axis_scale(bool set); //do not adjust scale values, default: true
	
	void set_option_stop_on_full(bool set); //stop recording when buffers become full, default: false
	
	void set_option_auto_extend_range_x(bool set); //extend index range to show all existing data. default: false
	void set_option_auto_set_range_y(unsigned int index, bool set); //if not, the user must set the range for each area. default: true
	void set_option_auto_set_zero_bottom(unsigned int index, bool set); //set the bottom of range y to zero in auto mode. default: true
	
	void set_option_show_axis_x_values(bool set); //only that of the bottommost is shown. default: true
	void set_option_axis_x_int_values(bool set); //don't show decimal digits for x-axis values. default: false
	void set_option_show_axis_y_values(bool set); //shown in left border of each area. default: true
	
	void set_option_show_average_line(unsigned int index, bool set); //this requires extra calculation, though it was optimized
	
	void set_option_anti_alias(bool set); //font of x-axis, y-axis values are not influenced. default: false
	
private:
	unsigned int var_cnt = 0;
	
	VariableAccessPtr* ptrs = NULL;
	CircularBuffer* bufs = NULL;
	PlotArea* areas = NULL; Gtk::EventBox* eventboxes = NULL; //DrawingArea can't handle button events anyway
	
	Gtk::Box scrollbox; Gtk::Scrollbar scrollbar; Gtk::Label space_left_of_scroll;
	Gtk::Box box_var_names; Gtk::Label* var_labels; Gtk::Label label_cursor_x, label_axis_x_unit;
	Glib::Dispatcher dispatcher_refresh_indicators; volatile bool flag_refresh_scroll = false;
	
	std::thread* thread_record = NULL,
	           * thread_refresh = NULL;
	volatile bool flag_recording = false;
	bool flag_spike_check = false; //determined by buf_size > Plot_Data_Amount_Limit_Min
	bool option_stop_on_full = false;
	float interval = 10; unsigned int redraw_interval = 40; //in milliseconds
	std::chrono::system_clock::time_point tp_start;
	
	bool option_extend_index_range = false;
	volatile bool flag_goto_end = false, flag_extend = false;
	
	volatile bool flag_full = false;
	sigc::signal<void()> sig_full;
	Glib::Dispatcher dispatcher_sig_full;
	
	volatile bool flag_sync_buf_plot = false;
	
	float axis_x_unit = 0; bool flag_axis_x_unique_unit = false;
	std::string axis_x_unit_name = "";
	
	volatile bool flag_cursor = false; volatile float cursor_x = 0;
	std::ostringstream oss; //used to show x,y values at the cursor's location
	
	void record_loop();
	void refresh_loop();
	
	void on_scroll();
	bool on_mouse_click(GdkEventButton* event);
	bool on_motion_notify(GdkEventMotion* motion_event);
	bool on_leave_notify(GdkEventCrossing* crossing_event);
	
	void refresh_indicators();
	void refresh_areas(bool forced_check_range_y = false, bool forced_adapt = false);
	bool auto_set_scroll_mode();
	bool auto_set_scroll_mode(Glib::RefPtr<Gtk::Adjustment> adj);
	
	void refresh_var_labels();
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

inline IndexRange Recorder::data_range() const
{
	return this->bufs[0].range();
}

inline unsigned int Recorder::data_count_max() const
{
	return this->bufs[0].size();
}

inline IndexRange Recorder::data_range_max() const
{
	return this->bufs[0].range_max();
}

inline float Recorder::t_data(unsigned int i) const
{
	return (this->bufs[0].count_overwritten() + i) * this->axis_x_unit;
}

inline float Recorder::t_first_data() const
{
	return this->t_data(0);
}

inline float Recorder::t_last_data() const
{
	return this->t_data(this->data_count() - 1);
}

inline std::chrono::system_clock::time_point Recorder::time_start() const
{
	return this->tp_start;
}

inline std::chrono::system_clock::time_point Recorder::time_data(unsigned int i) const
{
	return this->tp_start + std::chrono::microseconds(
		(long int)((this->bufs[0].count_overwritten() + i) * 1000.0 * this->interval)
	);
}

inline std::chrono::system_clock::time_point Recorder::time_first_data() const
{
	return this->time_data(0);
}

inline std::chrono::system_clock::time_point Recorder::time_last_data() const
{
	return this->time_data(this->data_count() - 1);
}

inline CircularBuffer& Recorder::buffer(unsigned int index) const
{
	return this->bufs[index];
}

inline IndexRange Recorder::axis_x_range() const
{
	return this->areas[0].get_range_x();
}

inline ValueRange Recorder::axis_y_range(unsigned int index) const
{
	if (index > this->var_cnt - 1) return ValueRange(0, 0);
	return this->areas[index].get_range_y();
}

inline void Recorder::refresh_view()
{
	this->set_axis_x_range();
}

inline bool Recorder::set_axis_x_range(unsigned int range_width)
{
	IndexRange range(0, range_width);
	if (this->flag_goto_end && range_width < this->data_range().max())
		range.max_move_to(this->data_range().max());
	return this->set_axis_x_range(range);
}

inline bool Recorder::set_axis_x_range(unsigned int min, unsigned int max)
{
	return this->set_axis_x_range(IndexRange(min, max));
}

/*------------------------------ private functions ------------------------------*/

inline bool Recorder::auto_set_scroll_mode()
{
	return this->auto_set_scroll_mode(this->scrollbar.get_adjustment());
}

inline void Recorder::refresh_areas(bool forced_check_range_y, bool forced_adapt)
{
	for (unsigned int i = 0; i < this->var_cnt; i++)
		this->areas[i].refresh(forced_check_range_y, forced_adapt, this->flag_sync_buf_plot);
	this->flag_sync_buf_plot = false;
}

}

#endif

