prefix = .
CPPFLAGS = -O3 -flto

includedir = $(prefix)/include
includedir_subdir = $(includedir)/simple-cairo-plot

libdir = $(prefix)/lib
target = $(libdir)/libsimple-cairo-plot.a

# use gcc-ar for LTO support
AR = gcc-ar
ifeq '$(findstring ;,$(PATH))' ';'
# Windows (neither MSYS2 nor Cygwin)
MKDIR = mkdir
CP = copy
RM = del /Q
RMDIR = rmdir /S /Q
target_demo = plot_demo.exe
else
MKDIR = mkdir -p
CP = cp
RM = rm -f
RMDIR = rm -f -r
target_demo = plot_demo
endif

cpp_options = -I$(includedir) `pkg-config gtkmm-3.0 --cflags --libs` $(CPPFLAGS)
headers = $(foreach h, $(wildcard *.h), $(includedir_subdir)/$(h))
objects = circularbuffer.o plottingarea.o recorder.o frontend.o

$(target): $(headers) $(objects) $(libdir)
	$(AR) rcs $@ $(objects)

$(target_demo): demo.cpp $(target)
	$(CXX) $< -L$(libdir) -lsimple-cairo-plot $(cpp_options) -o $@

%.o: %.cpp
	$(CXX) -c $< $(cpp_options)

$(libdir):
	-$(MKDIR) $@

$(includedir_subdir):
	-$(MKDIR) $@

$(includedir_subdir)/%.h: $(includedir_subdir) %.h
	$(CP) $(@F) $<

demo: $(target_demo)
	./$<

.PHONY: clean
clean:
	-$(RMDIR) lib include
	-$(RM) *.o target_demo
