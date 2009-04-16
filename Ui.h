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

#ifndef UI_H
#define UI_H

#include "Common.h"

#include <string>
#include <SDL/SDL.h>

class Canvas;
class Widget;

class WidgetParent
{
 public:
  virtual void add( Widget* w )=0;
  virtual void remove( Widget* w )=0;
};

class Widget
{
 public:
  virtual ~Widget() {} 
  virtual bool isDirty() {return false;}
  virtual Rect dirtyArea() {return Rect();};
  virtual void onTick( int tick ) {}
  virtual void draw( Canvas& screen, const Rect& area ) {};
  virtual bool handleEvent( SDL_Event& ev ) {return false;}
  void setParent(WidgetParent* p) {m_parent = p;}
  WidgetParent* parent() { return m_parent; }
 protected:
  WidgetParent* m_parent;
  Rect          m_pos;
};

class Label : public Widget
{
 public:
  Label();
  void text( std::string& s );
  void align( int a );
 private:
  
};

class Button : public Widget
{
 public:
  Button();
};

class Container : public Widget, public WidgetParent
{
 public:
  Container();
  
  virtual bool isDirty();
  virtual Rect dirtyArea();
  virtual void onTick( int tick );
  virtual void draw( Canvas& screen, const Rect& area );
  virtual bool handleEvent( SDL_Event& ev );

  void add( Widget* w );
  virtual void remove( Widget* w );
};


class Layer : public Widget
{
 public:
  virtual void onShow() {}
  virtual void onHide() {}
};


#endif //UI_H
