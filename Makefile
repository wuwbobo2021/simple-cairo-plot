prefix = .
CPPFLAGS = -O3 -flto

includedir = $(prefix)/include
includedir_subdir = $(includedir)/simple-cairo-plot
headers = $(foreach h, $(wildcard *.h), $(includedir_subdir)/$(h))

cpp_options = -I$(includedir) `pkg-config gtkmm-3.0 --cflags --libs` $(CPPFLAGS)
objects = circularbuffer.o plottingarea.o recorder.o frontend.o

libdir = $(prefix)/lib
target = $(libdir)/libsimple-cairo-plot.a

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

$(target): $(headers) $(objects) $(libdir)
	$(AR) rcs $(libdir)/libsimple-cairo-plot.a $(objects)

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
	-$(RM) *.o *.exe
