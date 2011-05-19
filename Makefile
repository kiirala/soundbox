GENERAL=-O0 -g -march=native -Wall -Wextra
CXXFLAGS=`pkg-config --cflags sdl` $(GENERAL) -MMD -MP
# gtkmm-2.4
LDFLAGS=`pkg-config --libs sdl` $(GENERAL)
# gtkmm-2.4

OBJECTS=sdlmain.o
PROGRAMS=sdlmain

all: $(PROGRAMS)

-include $(OBJECTS:.o=.d)

clean:
	rm -f $(PROGRAMS) $(OBJECTS) $(OBJECTS:.o=.d) *~


