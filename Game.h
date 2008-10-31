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

#ifndef GAME_H
#define GAME_H

#include "Levels.h"

class Canvas;

struct GameControl
{
  GameControl() : m_quit(false),
		 m_edit( false ),
		 m_refresh( true ),
                 m_fade(false),
		 m_colour( 2 ),
		 m_strokeFixed( false ),
		 m_strokeSleep( false ),
		 m_strokeDecor( false ),
		 m_levels(),
                 m_level(0)
  {}
  virtual ~GameControl() {}
  virtual bool save( const char *file=NULL ) { return false; }
  virtual bool send() { return false; }
  virtual bool load( const char* file ) { return false; }
  virtual void gotoLevel( int l, bool replay=false ) {}
  Levels& levels() { return m_levels; }
  virtual bool  renderScene( Canvas& c, int level ) =0; 
  bool  m_quit;
  bool  m_edit;
  bool  m_refresh;
  bool  m_fade;
  int   m_colour;
  bool  m_strokeFixed;
  bool  m_strokeSleep;
  bool  m_strokeDecor;
  Levels m_levels;
  int    m_level;
};


#endif //GAME_H
