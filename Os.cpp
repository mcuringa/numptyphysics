/*
 * This file is part of NumptyPhysics
 * Copyright (C) 2008 Tim Edmonds
 * 
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include "Os.h"
#include <SDL/SDL.h>


static const BasicEventMap::KeyPair game_keymap[] = {
  { SDLK_SPACE,    Event::PAUSE },
  { SDLK_KP_ENTER, Event::PAUSE },
  { SDLK_RETURN,   Event::PAUSE },
  { SDLK_ESCAPE,   Event::UNDO  },
  { SDLK_s,        Event::SAVE  },
  { SDLK_F4,       Event::MENU  },
  { SDLK_e,        Event::EDIT  },
  { SDLK_F6,       Event::EDIT  },
  { SDLK_r,        Event::RESET },
  { SDLK_UP,       Event::RESET },
  { SDLK_n,        Event::NEXT  },
  { SDLK_RIGHT,    Event::NEXT  },
  { SDLK_p,        Event::PREVIOUS },
  { SDLK_LEFT,     Event::PREVIOUS },
  { SDLK_v,        Event::REPLAY},
  {}
};

static const BasicEventMap::ButtonPair game_mousemap[] = {
  { SDL_BUTTON_LEFT, Event::DRAWBEGIN, Event::DRAWMORE, Event::DRAWEND },
  {}
};


static const BasicEventMap::KeyPair app_keymap[] = {
  { SDLK_q, Event::QUIT },
  {}
};



static const BasicEventMap::ButtonPair edit_mousemap[] = {
  { SDL_BUTTON_LEFT, Event::DRAWBEGIN, Event::DRAWMORE, Event::DRAWEND },
  { SDL_BUTTON_RIGHT, Event::MOVEBEGIN, Event::MOVEMORE, Event::MOVEEND },
  { SDL_BUTTON_MIDDLE, Event::DELETE },
  {}
};


class AppMap : public BasicEventMap
{
public:
  AppMap() : BasicEventMap( app_keymap, NULL ) {}
  Event process(const SDL_Event& ev)
  {
    if (ev.type==SDL_QUIT) {
      return Event(Event::QUIT);
    }
    return BasicEventMap::process(ev);
  }

};



EventMap* Os::getEventMap( EventMapType type )
{
  static BasicEventMap gameMap(game_keymap,game_mousemap);
  static BasicEventMap editMap(NULL,edit_mousemap);
  static AppMap appMap;

  switch (type) {
  case GAME_MAP:
    return &gameMap;
  case APP_MAP:
    return &appMap;
  case EDIT_MAP:
    return &editMap;
  }
  return NULL;
}
