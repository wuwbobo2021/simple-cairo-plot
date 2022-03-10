# SimpleCairoPlot
A simple (for the ease of use) Gtk component for continuous plotting on the graph. it depends on `gtkmm-3.0`, and is implemented with Cairo. If you need advanced features, please look for other projects.

## Classes
`AxisRange`: Closed range between two `float` values. It supports many operations, including mapping of a given value to another range. All of it's functions are inlined.

`CircularBuffer`: Where the data should be pushed back to update the plotting in `PlottingArea`. After it's full, it discards an item each time a new item is pushed into, but it avoids moving every item in the memory region. Most of its simple functions are inlined.

`PlottingArea`: Implements a graph for a single buffer without scroll box. It only supports a single variable, and values should be pushed into its buffer manually.

`Logger`: The ultimate class in this component (it will be implemented later), with multiple plotting areas and a scroll box for axis-x arranged vertically. It accepts a group of pointers of variables or functions from which the data is read, then creates an array of buffers and an array of plotting areas for these variables. After it's member function `start()` is called, it reads data and pushs the data into the buffers automatically according to the given interval, and the unit of axis-x values is set to seconds.

## Test
```
git clone https://github.com/wuwbobo2021/cairo-plot
cd cairo-plot
g++ circularbuffer.cpp plottingarea.cpp plottingarea_test.cpp -o test_program -O3 `pkg-config gtkmm-3.0 --cflags --libs`
./test_program
```
You can modify `plottingarea_test.cpp` to change the wave and make other adjustments, like speed, buffer size, or axis-y range.

