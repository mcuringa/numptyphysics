#ifndef SWIPE_H
#define SWIPE_H

#include <SDL_syswm.h>

class Swipe {
    public:
        static void lock(bool locked);
        static SDL_SysWMinfo m_syswminfo;
};

#endif
