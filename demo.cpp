#include <cmath> //sin(), fabs()
#include <iostream>
#include <chrono>

#include "frontend.h"

using namespace std;
using namespace std::chrono;
using namespace std::this_thread;

using namespace SimpleCairoPlot;

class Demo
{
	Frontend frontend;
	unsigned int freq = 2; //Hz
	system_clock::time_point t_start;
	
	float t();
	float read_var1();
	float read_var2();

public:
	Demo();
	void run();
};

Demo::Demo()
{
	std::vector<VariableAccessPtr> vect_ptr;
	VariableAccessPtr ptr1 = MemberFuncPtr<Demo, &Demo::read_var1>(this),
	                  ptr2 = MemberFuncPtr<Demo, &Demo::read_var2>(this);
	ptr1.color_plot.set_rgba(1.0, 0.0, 0.0); //red
	ptr2.color_plot.set_rgba(0.0, 0.0, 1.0); //blue
	vect_ptr.push_back(ptr1); vect_ptr.push_back(ptr2);
	this->frontend.init(vect_ptr, 1000); //size of both buffers will be 1000
}

void Demo::run()
{
	this->t_start = system_clock::now();
	this->frontend.open();
	this->frontend.recorder().set_interval(2);
	this->frontend.run(); //blocks
}

inline float Demo::t()
{
	time_point t_now = system_clock::now();
	return (t_now - this->t_start).count() / 1000.0 / 1000.0 / 1000.0; //nanoseconds to seconds
}

float Demo::read_var1()
{
	float cycles = this->t() * (2.0*this->freq);
	float per = cycles - (int)cycles; // [0, 1)
	if (per > 0.6) return 0; else return 1;
}

float Demo::read_var2()
{
	if (! this->read_var1()) return 0;
	
	const float pi = 3.141592654;
	float cycles = this->t() * this->freq;
	float psi = (cycles - (int)cycles) * 2*pi;
	return fabs(10 * sin(psi));
}

int main(int argc, char** argv)
{
	Demo demo;
	demo.run();
	return 0;
}

