# SimpleCairoPlot
An easy-to-use component for continuous recording and plotting of variables. it depends on `gtkmm-3.0`, and is implemented with Cairo.

## Demo
To ceeate static library:
```
git clone https://github.com/wuwbobo2021/simple-cairo-plot
mv simple-cairo-plot/demo.cpp .
cd simple-cairo-plot
g++ -c circularbuffer.cpp plottingarea.cpp recorder.cpp frontend.cpp `pkg-config gtkmm-3.0 --cflags --libs` -O3
ar rcs libsimple-cairo-plot.a circularbuffer.o plottingarea.o recorder.o frontend.o
cd ..
g++ demo.cpp -I./simple-cairo-plot -L./simple-cairo-plot -lsimple-cairo-plot `pkg-config gtkmm-3.0 --cflags --libs` -O3 -o test_program
./test_program
```
To create shared library:
```
git clone https://github.com/wuwbobo2021/simple-cairo-plot
mv simple-cairo-plot/demo.cpp .
cd simple-cairo-plot
g++ circularbuffer.cpp plottingarea.cpp recorder.cpp frontend.cpp `pkg-config gtkmm-3.0 --cflags --libs` -O3 -shared -fPIC -o ../libsimple-cairo-plot.so
cd ..
g++ demo.cpp `pkg-config gtkmm-3.0 --cflags --libs` -I./simple-cairo-plot -L. -lsimple-cairo-plot -O3 -o test_program
./test_program
```
You can modify `demo.cpp` to change the wave form and make other adjustments, like speed, buffer size or axis-y range.

For 64-bit Windows, refer to https://github.com/tschoonj/GTK-for-Windows-Runtime-Environment-Installer.

For 32-bit Windows:
1. Look for a mirror of MSYS2 in https://www.msys2.org/dev/mirrors;
2. Download `msys2-i686-latest.sfx.exe`, place it under a short path and extract it;
3. Follow the instruments in https://wiki.gnome.org/Projects/gtkmm/MSWindows.

Note: On Windows, the output executable must have `.exe` suffix.

## Classes
`AxisRange`: Closed range between two `float` values. It supports many operations, including mapping of a given value to another range. All of it's functions are inlined.

`CircularBuffer`: Where the data should be pushed back to update the plotting in `PlottingArea`. After it's full, it discards an item each time a new item is pushed into, but it avoids moving every item in the memory region. Most of its simple functions are inlined.

`PlottingArea`: Implements a graph box for a single buffer without scroll box. It only supports a single variable, and values should be pushed into its buffer manually.

`VariableAccessPtr`: Pointer of an variable or a function which has a `void*` parameter and returns a `float` value. Pointer of a member function of class `T` which returns a `float` value and has no extra parameters can be created by:
```
MemberFuncPtr<typename T, float (T::*F)()>(T* pobj)
```

`Recorder`: Packs multiple plotting areas and a scroll box for axis-x. It accepts a group of `VariableAccessPtr` pointers from which the data is read, then creates multiple buffers multiple plotting areas for these variables. After it is started, it reads and pushs the data into the buffers in given interval, and the unit of axis-x values is set to seconds. Provides zoom in/out (by left/right mouse button clicking on it) and CSV file opening/saving features.

`Frontend`: Provides a simple interface to create a Gtk application and a window for the recorder. It runs in a seperate thread for Gtk, until it is destructed or its member function `close()` is being called. Call its member function `run()` to join the Gtk thread, then it will run until the window is closed. Notice: Through function `recorder()` you can get a reference of the `Recorder` object as a Gtk Widget after the window is opened, but the object itself will be destructed when the window is being closed.

## Known Problems
1. On Windows, segmentation fault will be produced when the thread created by `Frontend` exits. Use `Frontend::run()` instead of `Frontend::open()` (which creates a new thread for `Gtk::Application::run()`) on Windows, especially if you need to do nessecary things after the window is closed by the user.
2. It seems like the record process can be interrupted by the environment, this causes missing of data and unsmooth curves on the graph. It works very well on XFCE, and is acceptable on GNOME and KDE, but the unsmooth effect can be significant on Windows that the delay can sometimes exceed 20 ms.
3. Limited by software timer accuracy, it is IMPOSSIBLE for the recorder to keep its data sampling frequency higher than 10 kHz (0.1 ms interval). The higher the frequency, the lower the stability.
4. It has not been migrated to `gtkmm-4.0`, partly because the repository of newest distributions of Debian and Ubuntu don't contain this new version.
