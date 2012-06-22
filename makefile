
APP = numptyphysics

DESTDIR ?=
PREFIX = /opt/numptyphysics

CXXFLAGS += -DINSTALL_BASE_PATH=\"$(PREFIX)/data\"

SOURCES = $(wildcard *.cpp)

all: $(APP)

# Required modules (uses pkg-config)
PKGS = sdl SDL_image

CXXFLAGS += $(shell pkg-config --cflags $(PKGS))
LIBS += $(shell pkg-config --libs $(PKGS))

# There's no pkg-config module for SDL_ttf?
LIBS += -lSDL_ttf


# Box2D Library
CXXFLAGS += -IBox2D/Include
BOX2D_SOURCE := Box2D/Source
BOX2D_LIBRARY := Gen/float/libbox2d.a
LIBS += $(BOX2D_SOURCE)/$(BOX2D_LIBRARY)

$(BOX2D_SOURCE)/$(BOX2D_LIBRARY):
	$(MAKE) -C $(BOX2D_SOURCE) $(BOX2D_LIBRARY)


# Dependency tracking
DEPENDENCIES = $(SOURCES:.cpp=.d)
CXXFLAGS += -MD
-include $(DEPENDENCIES)


# Pick the right OS-specific module here
SOURCES += os/OsFreeDesktop.cpp
CXXFLAGS += -I.

OBJECTS = $(SOURCES:.cpp=.o)


$(APP): $(OBJECTS) $(BOX2D_SOURCE)/$(BOX2D_LIBRARY)
	$(CXX) -o $@ $^ $(LIBS)


clean:
	rm -f $(OBJECTS)
	rm -f $(DEPENDENCIES)
	$(MAKE) -C Box2D/Source clean

distclean: clean
	rm -f $(APP)

install: $(APP)
	mkdir -p $(DESTDIR)/$(PREFIX)/bin
	install -m 755 $(APP) $(DESTDIR)/$(PREFIX)/bin/
	mkdir -p $(DESTDIR)/usr/share/applications
	install -m 644 $(APP).desktop $(DESTDIR)/usr/share/applications/
	cp -rpv data $(DESTDIR)/$(PREFIX)/


.PHONY: all clean distclean
.DEFAULT: all

