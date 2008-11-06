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

#include "Overlay.h"
#include "Canvas.h"
#include "Game.h"
#include "Config.h"

#include <SDL/SDL.h>
#include <string>

extern Rect FULLSCREEN_RECT;

class OverlayBase : public Overlay
{
public:
  OverlayBase( GameControl& game, int x=10, int y=10, bool dragging_allowed=true )
    : m_game(game),
      m_x(x), m_y(y),
      m_canvas(NULL),
      m_isDirty(true),
      m_dragging(false),
      m_dragging_allowed(dragging_allowed),
      m_buttonDown(false)
  {}
  virtual ~OverlayBase() 
  {
    delete m_canvas;
  }

  bool isDirty()
  {
    return m_isDirty;
  }

  Rect dirtyArea() 
  {
    return Rect(m_x,m_y,m_x+m_canvas->width()-1,m_y+m_canvas->height()-1);
  }
 
  virtual void onShow() { dirty(); }
  virtual void onHide() {}
  virtual void onTick( int tick ) {}

  virtual void draw( Canvas& screen )
  {
    if ( m_canvas ) {
      screen.drawImage( m_canvas, m_x, m_y );
    }
    m_isDirty = false;
  }

  virtual bool handleEvent( SDL_Event& ev )
  {
    switch( ev.type ) {      
    case SDL_MOUSEBUTTONDOWN:
      //printf("overlay click\n"); 
      if ( ev.button.button == SDL_BUTTON_LEFT
	   && ev.button.x >= m_x && ev.button.x <= m_x + m_canvas->width() 
	   && ev.button.y >= m_y && ev.button.y <= m_y + m_canvas->height() ) {
	m_orgx = ev.button.x;
	m_orgy = ev.button.y;
	m_prevx = ev.button.x;
	m_prevy = ev.button.y;
	m_buttonDown = true;
	return true;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if ( ev.button.button == SDL_BUTTON_LEFT ) {
	if ( m_dragging ) {
	  m_dragging = false;
	} else if ( Abs(ev.button.x-m_orgx) < CLICK_TOLERANCE
		    && Abs(ev.button.y-m_orgy) < CLICK_TOLERANCE ){
	  onClick( m_orgx-m_x, m_orgy-m_y );
	}
	m_buttonDown = false;
      }
      break;
    case SDL_MOUSEMOTION:
      if ( !m_dragging
	   && m_buttonDown
           && m_dragging_allowed
	   && ( Abs(ev.button.x-m_orgx) >= CLICK_TOLERANCE
		|| Abs(ev.button.y-m_orgy) >= CLICK_TOLERANCE ) ) {
	m_dragging = true;
      }
      if ( m_dragging ) {
	m_x += ev.button.x - m_prevx;
	m_y += ev.button.y - m_prevy;
	m_prevx = ev.button.x;
	m_prevy = ev.button.y;
	m_game.m_refresh = true;
      }
      break;
    default:
      break;
    }
    return false;    
  }
  
protected:
  void dirty()
  {
    m_isDirty = true;
  }

  void dirty( const Rect& r )
  {
    m_isDirty = true;
  }

  virtual bool onClick( int x, int y ) {
    for ( int i=m_hotSpots.size()-1; i>=0; i-- ) {
      if ( m_hotSpots[i].rect.contains( Vec2(x,y) )
	   && onHotSpot( m_hotSpots[i].id ) ) {
	return true;
      }
    }
    return false; 
  }

  virtual bool onHotSpot( int id ) { return false; }

  void addHotSpot( const Rect& r, int id )
  {
    HotSpot hs = { r, id };
    m_hotSpots.append( hs );
  }

  struct HotSpot { Rect rect; int id; };
  GameControl& m_game;
  int     m_x, m_y;
  Canvas *m_canvas;
private:
  int     m_orgx, m_orgy;
  int     m_prevx, m_prevy;
  bool    m_isDirty;
  bool    m_dragging;
  bool    m_dragging_allowed;
  bool    m_buttonDown;
  Array<HotSpot> m_hotSpots;
};


class IconOverlay: public OverlayBase
{
  std::string m_filename;
public:
  IconOverlay(GameControl& game, const char* file, int x=100,int y=20, bool dragging_allowed=true)
    : OverlayBase(game,x,y,dragging_allowed),
      m_filename( file )
  {
    m_canvas = new Image( m_filename.c_str() );
  }
};


class NextLevelOverlay : public IconOverlay
{
public:
  NextLevelOverlay( GameControl& game )
    : IconOverlay(game,"next.png",
		  FULLSCREEN_RECT.centroid().x-200,
		  FULLSCREEN_RECT.centroid().y-120),
      m_levelIcon(-2),
      m_icon(NULL)
  {
    addHotSpot( Rect(0,    0,100,180), 0 );
    addHotSpot( Rect(300,  0,400,180), 1 );
    addHotSpot( Rect(0,  180,200,240), 2 );
    addHotSpot( Rect(200,180,400,240), 3 );
  }
  ~NextLevelOverlay()
  {
    delete m_icon;
  }
  virtual void onShow()
  {
    IconOverlay::onShow();
    m_game.m_refresh = true; //for fullscreen fade
    m_game.m_fade = true;
    m_selectedLevel = m_game.m_level+1;
  }
  virtual void onHide()
  {
    IconOverlay::onHide();
    m_game.m_refresh = true; //for fullscreen fade
    m_game.m_fade = false;
  }
  virtual void draw( Canvas& screen )
  {
    IconOverlay::draw( screen );
    if ( genIcon() ) {
      screen.drawImage( m_icon, m_x+100, m_y+75 );
    } else {
      dirty();
    }
  }
  virtual bool onHotSpot( int id ) 
  {
    switch (id) {
    case 0: if (m_selectedLevel>0) m_selectedLevel--; break;
    case 1: m_selectedLevel++; break;
    case 2: m_game.gotoLevel( m_game.m_level,true ); break;
    case 3: m_game.gotoLevel( m_selectedLevel ); break;
    default: return false;
    }
    dirty();
    return true;
  } 
private:
  bool genIcon()
  {
    if ( m_icon==NULL || m_levelIcon != m_selectedLevel ) {
      delete m_icon;
      m_icon = NULL;
      if ( m_selectedLevel < m_game.m_levels.numLevels() ) {
	Canvas temp( SCREEN_WIDTH, SCREEN_HEIGHT );
	if ( m_game.renderScene( temp, m_selectedLevel) ) {
	  m_icon = temp.scale( ICON_SCALE_FACTOR );
	}
      } else {
	m_icon = new Image("theend.png");
	m_caption = "no more levels!";
	m_selectedLevel = m_game.m_levels.numLevels();
      }
      m_levelIcon = m_selectedLevel;
    }
    return m_icon;
  }
  int          m_selectedLevel;
  int          m_levelIcon;
  Canvas*      m_icon;
  std::string  m_caption;
};



class EditOverlay : public IconOverlay
{
  int m_saving, m_sending;
public:
  EditOverlay( GameControl& game )
    : IconOverlay(game,"edit.png"),
      m_saving(0), m_sending(0)
      
  {
    for ( int i=0; i<18; i++ ) {
      addHotSpot( pos(i), i );
    }    
    for ( int i=0; i<NUM_BRUSHES; i++ ) {
      m_canvas->drawRect( pos(i), m_canvas->makeColour(brushColours[i]), true );
    }
  }
  Rect pos( int i ) 
  {
    int c = i%3, r = i/3;
    return Rect( c*28+13, r*28+33, c*28+33, r*28+53 );
  }
  int index( int x, int y ) 
  {
    int r = (y-33)/28;
    int c = (x-13)/28;
    if ( r<0 || c<0 || c>2 ) return -1; 
    return r*3+c;
  }
  void outline( Canvas& screen, int i, int c ) 
  {
    Rect r = pos(i) + Vec2(m_x,m_y);
    r.tl.x-=2; r.tl.y-=2;
    r.br.x+=2; r.br.y+=2;
    screen.drawRect( r, c, false );
  }
  virtual void draw( Canvas& screen )
  {
    IconOverlay::draw( screen );
    outline( screen, m_game.m_colour, 0 );
    if ( m_game.m_strokeFixed ) outline( screen, 12, 0 );
    if ( m_game.m_strokeSleep ) outline( screen, 13, 0 );
    if ( m_game.m_strokeDecor ) outline( screen, 14, 0 );
    if ( m_sending ) outline( screen, 16, screen.makeColour((m_sending--)<<9) );
    if ( m_saving )  outline( screen, 17, screen.makeColour((m_saving--)<<9) );
    if ( m_saving || m_sending ) dirty();
  }
  virtual bool onHotSpot( int i )
  {
    switch (i) {
    case 12: m_game.m_strokeFixed = ! m_game.m_strokeFixed; break;
    case 13: m_game.m_strokeSleep = ! m_game.m_strokeSleep; break;
    case 14: m_game.m_strokeDecor = ! m_game.m_strokeDecor; break;
    case 16: if ( m_game.send() ) m_sending=10; break;
    case 17: if ( m_game.save() ) m_saving=10; break;
    default: if (i<NUM_BRUSHES) m_game.m_colour = i; else return false;
    }
    dirty();
    return true;
  }
};




Overlay* createIconOverlay( GameControl& game, const char* file,
			    int x,int y,
			    bool dragging_allowed )
{
  return new IconOverlay( game, file, x, y, dragging_allowed );
}

extern Overlay* createEditOverlay( GameControl& game )
{
  return new EditOverlay( game );
}

extern Overlay* createNextLevelOverlay( GameControl& game )
{
  return new NextLevelOverlay( game );
}
