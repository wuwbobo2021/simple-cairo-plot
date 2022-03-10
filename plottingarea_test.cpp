// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.

#include <cmath> //sin()
#include <string>
#include <thread>
#include <chrono>

#include <gtkmm/window.h>
#include <gtkmm/button.h>
#include <gtkmm/box.h>

#include "plottingarea.h"

using namespace SimpleCairoPlot;

class WindowMain: public Gtk::Window
{
private:
	CircularBuffer data;
	PlottingArea plot_area;
	std::thread thread_data; bool flag_stop_thread = false;
	
	Gtk::Box box;
	Gtk::Box toolbox;
	Gtk::Button button_clear;
	
	void data_loop();
	void on_button_clear_clicked();
	
public:
	WindowMain();
	virtual ~WindowMain();
};

WindowMain::WindowMain():
	data(400 + 1),
	plot_area(this->data),
	thread_data(&WindowMain::data_loop, this),
	button_clear("Clear"),
	toolbox(Gtk::ORIENTATION_VERTICAL)
{
	set_title("Plotting Area Test");
	set_default_size(500, 300);
	
	add(this->box);
	this->box.set_border_width(5);
	
	this->plot_area.option_auto_set_range_y = false;
	this->plot_area.set_range_x(AxisRange(0, 400));
	this->plot_area.set_range_y(AxisRange(-20, 20));
	
	this->plot_area.set_axis_divider(5, 4);
	this->plot_area.option_show_axis_x_values = true;
	this->plot_area.option_show_axis_y_values = true;
	this->plot_area.set_axis_x_unit(0.005); // 5 ms per data
	
	this->box.pack_start(this->plot_area, Gtk::PACK_EXPAND_WIDGET);
	this->box.pack_start(this->toolbox, Gtk::PACK_SHRINK);
	this->toolbox.set_border_width(5);
	
	this->button_clear.signal_clicked().connect(sigc::mem_fun(*this, &WindowMain::on_button_clear_clicked));
	this->toolbox.pack_start(this->button_clear, Gtk::PACK_SHRINK);
	
	show_all_children();
}

WindowMain::~WindowMain()
{
	this->flag_stop_thread = true;
	this->thread_data.join();
}

void WindowMain::on_button_clear_clicked()
{
	this->data.clear();
}

void WindowMain::data_loop()
{
	using namespace std::this_thread;
	using namespace std::chrono;
	
	float x = 0;
	while (! this->flag_stop_thread) {
		time_point t = system_clock::now();
		this->data.push(10*sin(x) + (10/4)*sin(8*x));
		x += 0.05;
		sleep_until(t + milliseconds(5));
	}
}

int main(int argc, char** argv)
{
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create("org.gtkmm.examples.base");
	WindowMain window_main;
	return app->run(window_main, argc, argv);
}

