prefix = .
OPT = -O3 -flto

includedir = $(prefix)/include
includedir_subdir = $(includedir)/simple-cairo-plot

libdir = $(prefix)/lib
target = $(libdir)/libsimple-cairo-plot.a
target_demo = plot_demo

# use gcc-ar for LTO support
AR = gcc-ar

ifeq '$(findstring sh,$(SHELL))' 'sh'
# UNIX, MSYS2 or Cygwin
MKDIR = mkdir -p
CP = cp
RM = rm -f
RMDIR = rm -f -r
run_demo = ./$(target_demo)
else
# Windows, neither MSYS2 nor Cygwin
MKDIR = mkdir
CP = copy
RM = del /Q
RMDIR = rmdir /S /Q
run_demo = $(target_demo)
endif

CXXFLAGS = -I$(includedir) `pkg-config gtkmm-3.0 --cflags --libs` $(OPT)

# for demo program
LDFLAGS = -L$(libdir) -lsimple-cairo-plot $(CXXFLAGS)
ifeq '$(OS)' 'Windows_NT'
LDFLAGS += -mwindows
endif

headers = $(foreach h, $(wildcard *.h), $(includedir_subdir)/$(h))
objects = circularbuffer.o plotarea.o recorder.o frontend.o

$(target): $(headers) $(objects) $(libdir)
	$(AR) rcs $@ $(objects)

$(target_demo): demo.cpp $(target)
	$(CXX) $< $(LDFLAGS) -o $@

$(libdir):
	-$(MKDIR) $@

$(includedir_subdir):
	-$(MKDIR) $@

$(includedir_subdir)/%.h: $(includedir_subdir) %.h
	$(CP) $(@F) $<

demo: $(target_demo)
	$(run_demo)

.PHONY: clean
clean:
	-$(RMDIR) lib include
	-$(RM) *.o $(target_demo)
