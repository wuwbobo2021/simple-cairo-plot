# SimpleCairoPlot
An easy-to-use component for continuous recording and plotting of variables. it depends on `gtkmm-3.0`, and is implemented with Cairo.

## Demo
```
git clone https://github.com/wuwbobo2021/simple-cairo-plot
cd simple-cairo-plot
g++ circularbuffer.cpp plottingarea.cpp recorder.cpp frontend.cpp demo.cpp -o test_program -O3 `pkg-config gtkmm-3.0 --cflags --libs`
./test_program
```
For Windows, refer to https://github.com/tschoonj/GTK-for-Windows-Runtime-Environment-Installer.

You can modify `demo.cpp` to change the wave form and make other adjustments, like speed, buffer size or axis-y range.

If you would like to make use of this small component, you can compile the source files except `demo.cpp` into a shared library, and put the header files into a seperate folder, that will be the interface of the library.

## Classes
`AxisRange`: Closed range between two `float` values. It supports many operations, including mapping of a given value to another range. All of it's functions are inlined.

`CircularBuffer`: Where the data should be pushed back to update the plotting in `PlottingArea`. After it's full, it discards an item each time a new item is pushed into, but it avoids moving every item in the memory region. Most of its simple functions are inlined.

`PlottingArea`: Implements a graph box for a single buffer without scroll box. It only supports a single variable, and values should be pushed into its buffer manually.

`VariableAccessPtr`: Pointer of an variable or a function which has a `void*` parameter and returns a `float` value. Pointer of a member function of class `T` which returns a `float` value and has no extra parameters can be created by:
```
MemberFuncPtr<typename T, float (T::*F)()>(T* pobj)
```

`Recorder`: Packs multiple plotting areas and a scroll box for axis-x. It accepts a group of `VariableAccessPtr` pointers from which the data is read, then creates multiple buffers multiple plotting areas for these variables. After it is started, it reads and pushs the data into the buffers in given interval, and the unit of axis-x values is set to seconds. Provides scale in/out (by left/right mouse button clicking on it) and CSV file opening/saving features.

`Frontend`: Provides a simple interface to create a Gtk application and a window for the recorder. It runs in a seperate thread for Gtk, until it is destructed or its member function `close()` is being called. Call its member function `run()` to join the Gtk thread, then it will run until the window is closed. Notice: Through function `recorder()` you can get a reference of the `Recorder` object as a Gtk Widget after the window is opened, but the object itself will be destructed when the window is being closed.

