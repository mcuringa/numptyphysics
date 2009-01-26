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

#ifndef FONT_H
#define FONT_H

#include "Common.h"
#include "Path.h"
#include "Array.h"
#include <string>

class Canvas;

class Font
{
 public:
  Font( const std::string& file );
  Vec2 metrics( const std::string& text );
  void draw( Canvas* canvas, Vec2 pt, const std::string& text, int colour );
  Font* rescale( double scale );
 private:
  Path m_glyphs[128];
};


#endif //FONT_H
