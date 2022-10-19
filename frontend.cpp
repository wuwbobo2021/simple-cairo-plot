// by wuwbobo2021 <https://github.com/wuwbobo2021>, <wuwbobo@outlook.com>
// If you have found bugs in this program, please pull an issue, or contact me.
// Licensed under LGPL version 2.1.

#include <simple-cairo-plot/frontend.h>

#include <chrono>
#include <iostream>

#include <glibmm/stringutils.h>

using namespace std::chrono;
using namespace std::this_thread;

using namespace SimpleCairoPlot;

Frontend::Frontend() {}

Frontend::Frontend(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size)
{
	this->init(ptrs, buf_size);
}

void Frontend::init(std::vector<VariableAccessPtr>& ptrs, unsigned int buf_size)
{
	if (ptrs.size() == 0 || buf_size < 2)
		throw std::runtime_error("Frontend::init(): invalid parameter.");
	
	this->ptrs = ptrs; this->buf_size = buf_size;
}

#ifndef _WIN32

void Frontend::open()
{
	if (this->thread_gtk || this->window) return;
	this->thread_gtk = new std::thread(&Frontend::app_run, this);
	
	while (! this->window) sleep_for(milliseconds(10));
	sleep_for(milliseconds(10));
}

void Frontend::run()
{
	if (this->thread_gtk) {
		this->thread_gtk->join();
		delete this->thread_gtk; this->thread_gtk = NULL;
	} else
		this->app_run(); //run on current thread
}

void Frontend::close()
{
	if (! this->window) return;
	
	this->rec->stop();
	this->dispatcher_quit->emit(); //eventually deletes the dispatcher itself
	
	if (this->thread_gtk)
		this->run();
	else //this function is called from another thread too
		while (this->window) sleep_for(milliseconds(1));
}

Frontend::~Frontend()
{
	this->close();
}

#else

void Frontend::open()
{
	if (this->thread_gtk || this->window) return;
	
	if (! thread_gtk)
		thread_gtk = new std::thread(&Frontend::thread_loop, this);
	else
		this->flag_open = true;
	while (! this->window) sleep_for(milliseconds(10));
}

void Frontend::run()
{
	if (! this->thread_gtk)
		this->app_run();
	else {
		this->flag_open = true;
		while (this->flag_open)
			sleep_for(milliseconds(500));
	}
}

void Frontend::close()
{
	if (! this->window) return;
	
	this->rec->stop();
	this->dispatcher_quit->emit(); //eventually deletes itself
	
	while (this->window) sleep_for(milliseconds(10));
}

Frontend::~Frontend()
{
	if (! thread_gtk) return;
	
	if (this->window)
		this->dispatcher_quit->emit();
	this->flag_destruct = true;
	this->thread_gtk->detach();
	delete this->thread_gtk;
}

#endif

Recorder& Frontend::recorder() const
{
	unsigned int wait_ms = 0;
	while (!this->window && wait_ms < 5000) {
		sleep_for(milliseconds(10)); wait_ms += 10;
	}
	if (! this->window)
		throw std::runtime_error("Frontend::recorder(): frontend is not opened.");
	return *this->rec;
}

/*------------------------------ private functions ------------------------------*/

#ifdef _WIN32
void Frontend::thread_loop()
{
	while (! flag_destruct) {
		if (! flag_open) {
			sleep_for(milliseconds(200)); continue;
		}
		this->app_run();
		flag_open = false;
	}
	
	while (true) //to avoid segment fault, keep this thread until the real main thread exits
		sleep_for(seconds(10));
}
#endif

void Frontend::app_run()
{
	std::string app_name = "org.simple-cairo-plot.frontend_";
	app_name += std::to_string(steady_clock::now().time_since_epoch().count());
	Glib::RefPtr<Gtk::Application> app = Gtk::Application::create(app_name);
	
	this->dispatcher_quit = new Glib::Dispatcher;
	this->dispatcher_quit->connect(sigc::mem_fun(*(app.get()), &Gtk::Application::quit));
	
	this->create_window();
	this->create_file_dialog();
	
	app->run(*this->window);
	
	this->window = NULL; //the window is already destructed when the thread exits Application::run()
	delete this->file_dialog;
	delete this->dispatcher_quit;
}

void Frontend::create_window()
{
	this->rec = Gtk::manage(new Recorder(this->ptrs, this->buf_size));
	this->rec->signal_full().connect(sigc::mem_fun(*this, &Frontend::on_buffers_full));
	
	this->button_start_stop = Gtk::manage(new Gtk::Button("Start"));
	this->button_start_stop->signal_clicked().connect(
		sigc::mem_fun(*this, &Frontend::on_button_start_stop_clicked));
	
	Gtk::Button* button_open = Gtk::manage(new Gtk::Button("Open")),
	           * button_save = Gtk::manage(new Gtk::Button("Save"));
	button_open->signal_clicked().connect(sigc::mem_fun(*this, &Frontend::on_button_open_clicked));
	button_save->signal_clicked().connect(sigc::mem_fun(*this, &Frontend::on_button_save_clicked));
	
	Gtk::Box* box = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_HORIZONTAL)),
	        * bar = Gtk::manage(new Gtk::Box(Gtk::ORIENTATION_VERTICAL));
	
	bar->set_border_width(5); bar->set_spacing(5);
	bar->pack_start(*button_start_stop, Gtk::PACK_SHRINK);
	bar->pack_start(*button_open, Gtk::PACK_SHRINK);
	bar->pack_start(*button_save, Gtk::PACK_SHRINK);
	
	box->set_border_width(5);
	box->pack_start(*this->rec, Gtk::PACK_EXPAND_WIDGET);
	box->pack_start(*bar, Gtk::PACK_SHRINK);

	this->window = new Gtk::Window();
	this->window->set_title(this->title);
	this->window->set_default_size(640, 400);
	this->window->add(*box);
	this->window->show_all_children();
}

void Frontend::create_file_dialog()
{
	Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
	filter->set_name("CSV files (.csv)");
	filter->add_pattern("*.csv"); filter->add_mime_type("text/csv");
	
	this->file_dialog = new Gtk::FileChooserDialog("Open .csv file", Gtk::FILE_CHOOSER_ACTION_OPEN);
	this->file_dialog->set_select_multiple(false);
	this->file_dialog->add_filter(filter);
	this->file_dialog->add_button("_Cancel", Gtk::RESPONSE_CANCEL);
	this->file_dialog->add_button("_OK", Gtk::RESPONSE_OK);
	this->file_dialog->set_transient_for(*this->window);
	this->file_dialog->set_modal(true);
}

void Frontend::on_buffers_full()
{
	this->window->set_title(this->title + " - Full...");
}

void Frontend::on_button_start_stop_clicked()
{
	if (! this->rec->is_recording()) {
		this->rec->start();
		this->button_start_stop->set_label("Stop ");
		this->window->set_title(this->title + " - Recording...");
	} else {
		this->rec->stop();
		this->button_start_stop->set_label("Start");
		this->window->set_title(this->title);
	}
}

void Frontend::on_button_open_clicked()
{
	if (this->rec->is_recording()) this->on_button_start_stop_clicked();
	
	this->file_dialog->set_title("Open .csv file");
	this->file_dialog->set_action(Gtk::FILE_CHOOSER_ACTION_OPEN);
	Gtk::ResponseType resp = (Gtk::ResponseType) this->file_dialog->run();
	this->file_dialog->close();
	if (resp != Gtk::RESPONSE_OK) return;
	
	std::string path = this->file_dialog->get_file()->get_path();
	this->rec->open_csv(path);
}

void Frontend::on_button_save_clicked()
{
	using Glib::str_has_suffix;
	#ifdef _WIN32
		const std::string Slash = "\\";
	#else
		const std::string Slash = "/";
	#endif
	
	if (this->rec->is_recording()) this->on_button_start_stop_clicked();
	
	this->file_dialog->set_title("Save as .csv file");
	this->file_dialog->set_action(Gtk::FILE_CHOOSER_ACTION_SAVE);
	Gtk::ResponseType resp = (Gtk::ResponseType) this->file_dialog->run();
	this->file_dialog->close();
	if (resp != Gtk::RESPONSE_OK) return;
	
	std::string path = this->file_dialog->get_current_folder();
	if (! str_has_suffix(path, Slash)) path += Slash;
	path += this->file_dialog->get_current_name();
	if (! str_has_suffix(path, ".csv")) path += ".csv"; 
	
	bool suc = this->rec->save_csv(path);
	if (! suc) this->window->set_title(this->title + " - Failed to save as file");
}

