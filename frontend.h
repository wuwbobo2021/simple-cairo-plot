// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#ifndef SIMPLE_CAIRO_PLOT_FRONTEND_H
#define SIMPLE_CAIRO_PLOT_FRONTEND_H

#include <simple-cairo-plot/recorder.h>

#include <gtkmm/window.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>

namespace SimpleCairoPlot
{

class Frontend: public sigc::trackable
{
	std::vector<VariablePtr> ptrs; unsigned int buf_size;
	Recorder* rec;
	
	std::thread* thread_gtk = NULL;
	Glib::Dispatcher* dispatcher_quit = NULL;
	Gtk::Window* volatile window = NULL;
	
	Gtk::FileChooserDialog* file_dialog;
	Gtk::Button* button_start_stop;
	
#ifdef _WIN32
	volatile bool flag_open = true, flag_destruct = false;
	void thread_loop();
#endif
	
	void create_window();
	void create_file_dialog();
	void app_run();
	
	void on_buffers_full();
	void on_button_start_stop_clicked();
	void on_button_open_clicked();
	void on_button_save_clicked();
	
public:
	std::string title = "Recorder";
	
	Frontend(); void init(std::vector<VariablePtr>& ptrs, unsigned int buf_size);
	Frontend(std::vector<VariablePtr>& ptrs, unsigned int buf_size);
	Frontend(const Frontend&) = delete;
	Frontend& operator=(const Frontend&) = delete;
	virtual ~Frontend();
	
	void open(); //create a new thread to run the frontend
	Recorder& recorder() const; //notice: don't keep the returned reference when closing the frontend
	void run(); //run in current thread or join the existing frontend thread, blocks
	void close();
};

}
#endif
