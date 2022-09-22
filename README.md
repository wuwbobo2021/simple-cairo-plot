# SimpleCairoPlot
An easy-to-use component for continuous recording and plotting of variables. it depends on `gtkmm-3.0`, and is implemented with Cairo.

## Demo
Packages required: `gcc`, `make`, `git`(optional), `libgtkmm-3.0-dev`.
```
git clone https://github.com/wuwbobo2021/simple-cairo-plot
cd simple-cairo-plot
make demo
```
You can modify `demo.cpp` to change the wave form and make other adjustments, like speed, buffer size or axis-y range.

Install:
```
sudo make -e prefix=/usr
```

For 64-bit Windows, refer to: <https://github.com/tschoonj/GTK-for-Windows-Runtime-Environment-Installer>.

For 32-bit Windows:
1. Look for a mirror of MSYS2 in <https://www.msys2.org/dev/mirrors>;
2. Download `msys2-i686-latest.sfx.exe`, place it under a short path and extract it;
3. Follow the instruments in <https://wiki.gnome.org/Projects/gtkmm/MSWindows>.

## Classes
`AxisRange`: Closed range between two `float` values. It supports many operations, including mapping of a given value to another range. All of it's functions are inlined.

`CircularBuffer`: Where the data should be pushed back to update the graph in `PlottingArea`. After it becomes full, it discards an item each time a new item is pushed into, but it avoids moving every item in the memory region. Optimized algorithms calculating min/max/average values are implemented here, and spike detection is enabled by default so that spikes can be treated specially to avoid flickering of spikes when the x-axis index step for data plotting is adjusted for a wide index range. It is thread-safe, and most of its simple functions are inlined.

`PlottingArea`: Implements a graph box for a single buffer without scroll box. It only supports a single variable, and values should be pushed into its buffer manually. Axis ranges can be set either automatically or manually, and the grid with tick values can be either fixed or auto-adjusted. For x-axis index range, goto-end mode and extend mode are available.

`VariableAccessPtr`: Pointer of an variable or a function which has a `void*` parameter and returns a `float` value. Pointer of a member function of class `T` which returns a `float` value and has no extra parameters can be created by:
```
MemberFuncPtr<typename T, float (T::*F)()>(T* pobj)
```

`Recorder`: Packs multiple plotting areas and a scroll box for x-axis. It accepts a group of `VariableAccessPtr` pointers from which the data is read, then creates multiple buffers multiple plotting areas for these variables. After it is started, it reads and pushs the data into the buffers in given interval, and the unit of axis-x values is set to seconds. Provides zoom in/out (by left/right mouse button clicking on it) and CSV file opening/saving features.

`Frontend`: Provides a simple interface to create a Gtk application and a window for the recorder. It runs in a seperate thread for Gtk, until it is destructed or its member function `close()` is being called. Call its member function `run()` to join the Gtk thread, then it will run until the window is closed. Notice: Through function `recorder()` you can get a reference of the `Recorder` object as a Gtk Widget after the window is opened, but the object itself will be destructed when the window is being closed.

## Known Issues
1. On Windows, segmentation fault will be produced when the thread created by `Frontend` exits. Use `Frontend::run()` instead of `Frontend::open()` (which creates a new thread for `Gtk::Application::run()`) on Windows, especially if you need to do necessary things after the window is closed by the user.
2. The record process can be interrupted by the environment, this causes missing of data and unsmooth curves on the graph. It works very well on XFCE, and is acceptable on GNOME and KDE, but the unsmooth effect can be significant on Windows that the delay can sometimes exceed 20 ms.
3. Limited by software timer accuracy, it is IMPOSSIBLE for `Recorder` to keep its data sampling frequency higher than 10 kHz (0.1 ms interval). The higher the frequency, the lower the stability.
4. `Recorder` is not capable of loading a block of data at once. In this case, it can still be used to show data: do not call `Recorder::start()`, but load data into each buffer manually, then call `Recorder::set_axis_x_range()` to refresh the graph. In case of the amount of data in the buffers are not equal, that of the buffer for the first variable makes sense. To avoid writing invalid non-zero data into the CSV file, call `CircularBuffer::erase()` for each buffer after calling `Recorder::clear()`.
5. It has not been migrated to `gtkmm-4.0`, partly because the repository of newest distributions of Debian and Ubuntu has not provided this version of the library.
