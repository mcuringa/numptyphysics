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
#include "Path.h"
#include "Font.h"

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
      if ( m_hotSpots[i].rect.contains( Vec2(x,y) ) ) {
	if ( m_hotSpots[i].c ) {
	  m_hotSpots[i].c(m_hotSpots[i].o,m_hotSpots[i].f);
	  return true;
	} else if ( onHotSpot( m_hotSpots[i].id ) ) {
	  return true;
	}
      }
    }
    return false; 
  }

  virtual bool onHotSpot( int id ) { return false; }

  void addHotSpot( const Rect& r, int id )
  {
    HotSpot hs = { r, id, NULL, NULL, NULL };
    m_hotSpots.append( hs );
  }

  template<class C>
  void addHotSpot( const Rect& r, void (C::*func)() )
  {
    struct Caller {
      static void call( void* o, void (Overlay::*f)() ) {
	return (((C*)o)->*(void (C::*)())f)();
      }
    };
    HotSpot hs = { r, -1, this, (void (Overlay::*)())func, Caller::call };
    m_hotSpots.append( hs );
  }

  struct HotSpot 
  { 
    Rect rect;
    int id;
    void *o;
    void (Overlay::*f)(); 
    void (*c)(void*,void (Overlay::*)()); 
  };
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


class UiOverlay : public OverlayBase
{
public:
  UiOverlay(GameControl& game, const Rect& pos)
    : OverlayBase( game, pos.tl.x, pos.tl.y, true )
    , m_bgColour(0xf0f0a0)
    , m_fgColour(0)
    , m_motion(0)
  {
    m_canvas = new Canvas( pos.width(), pos.height() );
    m_canvas->setBackground( m_bgColour );
    m_canvas->clear();
  }

  void moveTo( const Vec2& dest )
  {
    if ( dest != Vec2(m_x,m_y) ) {
      m_motion = 10;
      m_dest = dest;
    }
  }

  void onTick( int tick )
  {
    if ( m_motion > 0 ) {
      Vec2 pos(m_x,m_y);
      pos *= m_motion-1;
      pos += m_dest;
      pos = pos /m_motion;
      m_x = pos.x;
      m_y = pos.y;
      m_motion--;
      m_game.m_refresh = 1;
    }
  }

  template<class C>
  void addButton( const char* icon, const Rect& r, void (C::*func)() )
  {
    if ( Config::findFile(icon) != icon ) {
      Image *i = new Image(icon);
      m_canvas->drawImage( i, r.tl.x, r.tl.y );
      delete i;
    } else {
      Path p(icon);
      p.translate( r.tl - p.bbox().tl );
      m_canvas->drawPath( p, m_fgColour, false );
    }
    addHotSpot( r, func );
  }
private:
  int  m_bgColour;
  int  m_fgColour;
  int  m_motion;
  Vec2 m_dest;
};


class MenuOverlay : public UiOverlay
{
public:
  MenuOverlay(GameControl& game)
    : UiOverlay(game,Rect(100,100,400,200))
  {
    addButton( "48,18 47,25 48,25 47,27 48,27 47,29 48,29 47,31 48,33 47,34 48,35 47,40 48,45 49,43 48,43 49,41 48,39 49,38 48,37 47,36 50,34 49,31 44,31 42,30 40,30 36,32 37,33 36,38 35,45 34,47 35,53 34,54 36,56 37,58 38,59 41,59 42,61 43,62 45,62 47,63 49,63 51,64 59,64 61,62 63,62 64,60 66,60 68,58 69,54 70,52 69,52 68,46 67,46 66,42 63,36 62,36 61,34 60,33 59,33 58,32 54,32 53,31 50,31 54,31 53,33", Rect(10,10,90,90), &MenuOverlay::doPause );
  }
  void doPause() {
    m_game.gotoLevel( m_game.m_level + 1 );
  }
  
  void onShow() { moveTo(Vec2(200,200)); }
  
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
    m_font = Config::font();
    m_font->draw( m_canvas, Vec2(130,20), "BRAVO!", 0 );
    m_font = Config::font()->rescale(0.5);
    m_font->draw( m_canvas, Vec2(30,220), "Action Replay", 0 );
    m_font->draw( m_canvas, Vec2(300,220), "Continue", 0 );

    addHotSpot( Rect(0,    0,100,180), &NextLevelOverlay::doPrevLevel );
    addHotSpot( Rect(300,  0,400,180), &NextLevelOverlay::doNextLevel );
    addHotSpot( Rect(0,  180,200,240), &NextLevelOverlay::doActionReplay );
    addHotSpot( Rect(200,180,400,240), &NextLevelOverlay::doContinue );
  }
  ~NextLevelOverlay()
  {
    delete m_icon;
    delete m_font;
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
      m_font->draw( &screen, Vec2(m_x+100,m_y+50), 
		    m_game.levels().levelName(m_selectedLevel), 0 );
      screen.drawImage( m_icon, m_x+100, m_y+75 );
    } else {
      dirty();
    }
  }
  void doPrevLevel()
  {
    if (m_selectedLevel>0) {
      m_selectedLevel--; 
      dirty();
    }
  }
  void doNextLevel()
  {
    m_selectedLevel++;
    dirty();
  }
  void doActionReplay()
  {
    m_game.gotoLevel( m_game.m_level,true );
  }
  void doContinue()
  {
    m_game.gotoLevel( m_selectedLevel );
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
  Font*        m_font;
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

Overlay* createEditOverlay( GameControl& game )
{
  return new EditOverlay( game );
}

Overlay* createNextLevelOverlay( GameControl& game )
{
  return new NextLevelOverlay( game );
}

Overlay* createMenuOverlay( GameControl& game )
{
  return new MenuOverlay( game );
}


