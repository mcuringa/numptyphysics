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

#ifndef EVENT_H
#define EVENT_H
#include <SDL/SDL.h>


struct Event
{

  enum Code {
    NOP,
    DRAWBEGIN,
    DRAWMORE,
    DRAWEND,
    MOVEBEGIN,
    MOVEMORE,
    MOVEEND,
    QUIT,
    EDIT,
    MENU,
    DELETE,
    NEXT,
    PREVIOUS,
    RESET,
    UNDO,
    PAUSE,
    REPLAY,
    SAVE
  } event;
  int  x,y;
  char c;

  Event(Code op, char cc=0) : event(op), c(cc) {}
  Event(Code op, int xx, int yy) : event(op), x(xx), y(yy) {}
};


struct EventMap
{
  virtual Event process(const SDL_Event& ev)=0;
};


class BasicEventMap : public EventMap
{
 public:
  struct KeyPair { SDLKey sym; Event::Code ev; };
  struct ButtonPair { uint8_t button; Event::Code down; Event::Code move; Event::Code up; };
  BasicEventMap( const KeyPair* keys, const ButtonPair* buttons );
  Event process(const SDL_Event& ev);
 protected:
  const KeyPair* lookupKey(SDLKey sym);
  const ButtonPair* lookupButton(uint8_t button);
 private:
  const KeyPair* m_keys;
  const ButtonPair* m_buttons;
};


enum EventMapType
{
  GAME_MAP,
  APP_MAP,
  EDIT_MAP
};

#endif //EVENT_H
