SOURCES=Path.cpp Canvas.cpp Levels.cpp Game.cpp Http.cpp happyhttp.cpp zoomer.cpp
#SOURCES=Savedialog.cpp

ARCH=$(shell uname -m)
BINDIR=$(ARCH)
TARGET=$(ARCH)/Game

OBJECTS=$(SOURCES:%.cpp=$(BINDIR)/%.o)
CCOPTS=-Wall -I Box2D/Include 
LDOPTS=-L$(BINDIR) -lSDL -lSDL_image -lX11

ifeq ($(MSYSTEM),MINGW32)
OBJECTS+=numptywinicon.o
LDOPTS+=-mwindows
CCOPTS+=-O3 -DARCH_i686 -D INSTALL_BASE_PATH=\"./data/\"
SOURCES+=OsWin32.cpp
else
ifeq ($(ARCH),i686)
CCOPTS+=-g -DARCH_i686
LDOPTS+=-g
SOURCES+=OsFreeDesktop.cpp
endif
ifeq ($(ARCH),x86_64)
CCOPTS+=-g -O3 -D ARCH_x86_64
#CCOPTS+=-g -D ARCH_x86_64 -DTARGET_FLOAT32_IS_FIXED
LDOPTS+=-g
SOURCES+=OsFreeDesktop.cpp
endif
ifeq ($(ARCH),arm)
CCOPTS+=-DARCH_arm -D USE_HILDON
# -DTARGET_FLOAT32_IS_FIXED
CCOPTS+=-O3 -fomit-frame-pointer -frename-registers -ffast-math
SOURCES+=OsHildon.cpp
#generic
#CCOPTS+=-mfpu=vfp -mfloat-abi=softfp 
#n8x0
CCOPTS+=-mcpu=arm1136j-s -mfpu=vfp -mfloat-abi=softfp 
#770
#CCOPTS+=-mcpu=arm1026ej-s -mfpu=vfp -mfloat-abi=softfp 
#LDOPTS+=-lm_vfp
#hildon bits
CCOPTS+=-I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include 
CCOPTS+=-I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include
CCOPTS+=-I/usr/include/hildon-1 
CCOPTS+=-I/usr/include/gtk-2.0 -I/usr/lib/gtk-2.0/include
CCOPTS+=-I/usr/include/atk-1.0 -I/usr/lib/atk-1.0/include
CCOPTS+=-I/usr/include/cairo
CCOPTS+=-I/usr/include/pango-1.0
CCOPTS+=-I/usr/include/hildon-fm-2
LDOPTS+=-losso -lossoemailinterface
LDOPTS+=-lhildonfm
endif
ifeq ($(ARCH),armv4tl)
CCOPTS+=-g -DARCH_armv4tl 
LDOPTS+=-g
SOURCES+=OsFreeDesktop.cpp
endif
endif

BOX2DLIB=$(BINDIR)/libbox2d.a
BOX2DSOURCES= \
	Box2D/Source/Collision/b2Distance.cpp \
	Box2D/Source/Collision/b2TimeOfImpact.cpp \
	Box2D/Source/Collision/b2CollideCircle.cpp \
	Box2D/Source/Collision/b2CollidePoly.cpp \
	Box2D/Source/Collision/Shapes/b2PolygonShape.cpp \
	Box2D/Source/Collision/Shapes/b2CircleShape.cpp \
	Box2D/Source/Collision/Shapes/b2Shape.cpp \
	Box2D/Source/Collision/b2PairManager.cpp \
	Box2D/Source/Collision/b2Collision.cpp \
	Box2D/Source/Collision/b2BroadPhase.cpp \
	Box2D/Source/Dynamics/b2WorldCallbacks.cpp \
	Box2D/Source/Dynamics/Joints/b2PrismaticJoint.cpp \
	Box2D/Source/Dynamics/Joints/b2MouseJoint.cpp \
	Box2D/Source/Dynamics/Joints/b2GearJoint.cpp \
	Box2D/Source/Dynamics/Joints/b2Joint.cpp \
	Box2D/Source/Dynamics/Joints/b2PulleyJoint.cpp \
	Box2D/Source/Dynamics/Joints/b2DistanceJoint.cpp \
	Box2D/Source/Dynamics/Joints/b2RevoluteJoint.cpp \
	Box2D/Source/Dynamics/Contacts/b2CircleContact.cpp \
	Box2D/Source/Dynamics/Contacts/b2PolyAndCircleContact.cpp \
	Box2D/Source/Dynamics/Contacts/b2Contact.cpp \
	Box2D/Source/Dynamics/Contacts/b2PolyContact.cpp \
	Box2D/Source/Dynamics/Contacts/b2ContactSolver.cpp \
	Box2D/Source/Dynamics/b2Island.cpp \
	Box2D/Source/Dynamics/b2Body.cpp \
	Box2D/Source/Dynamics/b2ContactManager.cpp \
	Box2D/Source/Dynamics/b2World.cpp \
	Box2D/Source/Common/b2BlockAllocator.cpp \
	Box2D/Source/Common/b2StackAllocator.cpp \
	Box2D/Source/Common/b2Settings.cpp \
	Box2D/Source/Common/b2Math.cpp
BOX2DOBJECTS=$(BOX2DSOURCES:%.cpp=$(BINDIR)/%.o)

all: $(TARGET) 

%_dirstamp:
	@mkdir -p $(@D)
	@echo t > $@

$(BOX2DLIB): $(BOX2DOBJECTS)
	ar r $@ $^
	ranlib $@

$(BINDIR)/%.o: %.cpp $(BINDIR)/%_dirstamp
	g++ -c $(CCOPTS) -MMD -MP -MF $(BINDIR)/$*.d $< -o $@

dummy:
	@cp $(BINDIR)/$*.d $(BINDIR)/$(*F).P; \
         sed -e 's/#.*//' -e 's/^[^:]*: *//' -e 's/ *\\$$//' \
              -e '/^$$/ d' -e 's/$$/ :/' \
              < $(BINDIR)/$*.d >> $(BINDIR)/$*.P; \
        rm -f $(BINDIR)/$*.d

numptywinicon.o: numptywinicon.res
	windres numptywinicon.res numptywinicon.o

clean:
	rm -fR $(BINDIR)

distclean: clean
	rm -f *~

$(TARGET): $(OBJECTS) $(BOX2DLIB)
	g++ $^ $(LDOPTS) -o $(TARGET)


-include $(OBJECTS:%.o=%.d)
-include $(BOX2DOBJECTS:%.o=%.d)
