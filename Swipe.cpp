
#include "Swipe.h"

#include <stdio.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

SDL_SysWMinfo
Swipe::m_syswminfo = {0};

void
Swipe::lock(bool locked)
{
    unsigned int customRegion[] = {
        0,
        0,
        locked?854:0,
        locked?480:0,
    };

    Atom customRegionAtom = XInternAtom(m_syswminfo.info.x11.display,
            "_MEEGOTOUCH_CUSTOM_REGION", False);

    XChangeProperty(m_syswminfo.info.x11.display,
            m_syswminfo.info.x11.window, customRegionAtom,
            XA_CARDINAL, 32, PropModeReplace,
            reinterpret_cast<unsigned char*>(&customRegion[0]), 4);

    XChangeProperty(m_syswminfo.info.x11.display,
            m_syswminfo.info.x11.fswindow, customRegionAtom,
            XA_CARDINAL, 32, PropModeReplace,
            reinterpret_cast<unsigned char*>(&customRegion[0]), 4);

    XChangeProperty(m_syswminfo.info.x11.display,
            m_syswminfo.info.x11.wmwindow, customRegionAtom,
            XA_CARDINAL, 32, PropModeReplace,
            reinterpret_cast<unsigned char*>(&customRegion[0]), 4);
}

