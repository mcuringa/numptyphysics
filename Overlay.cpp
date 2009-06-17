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
//#include "Worker.h"

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
    if ( m_isDirty ) {
      return Rect(m_x,m_y,m_x+m_canvas->width()-1,m_y+m_canvas->height()-1);
    } else {
      return Rect(0,0,0,0);
    }
  }
 
  virtual void onShow() { dirty(); }
  virtual void onHide() {}
  virtual void onTick( int tick ) {}

  virtual void draw( Canvas& screen, const Rect& area )
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
	  onDragStop( ev.button.x-m_x, ev.button.y-m_y );
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
	onDragStart( ev.button.x-m_x, ev.button.y-m_y );
      } else if ( m_dragging ) {
	onDrag( ev.button.x - m_prevx, ev.button.y - m_prevy );
	m_prevx = ev.button.x;
	m_prevy = ev.button.y;
      }
      break;
    case SDL_KEYDOWN:
      return onKey( ev.key.keysym.sym );

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

  virtual bool onKey( int k )
  {
    return false;
  }
  virtual bool onDragStart( int x, int y ) 
  {
    return false;
  }
  virtual bool onDragStop( int x, int y ) 
  {
    return false;
  }
  virtual bool onDrag( int dx, int dy ) 
  {
    m_x += dx;
    m_y += dy;
    m_game.m_refresh = true;
    return false;
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
    , m_bgColour(0xdfdfaf)
    , m_fgColour(0)
    , m_motion(0)
  {
    m_canvas = new Canvas( pos.width(), pos.height() );
    m_canvas->setBackground( m_bgColour );
    m_canvas->drawRect( 0, 0, pos.width(), pos.height(), 
			m_canvas->makeColour(0xffffff) );
    m_canvas->drawRect( 3, 3, pos.width()-6, pos.height()-6, 
			m_canvas->makeColour(m_bgColour) );
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
      if ( p.size() > 1 ) {
	p.translate( r.tl - p.bbox().tl );
	m_canvas->drawPath( p, m_fgColour, false );
      } else {
	Font::headingFont()->drawCenter( m_canvas, r.centroid(),
					 icon, m_fgColour );	
      }
    }
    addHotSpot( r, func );
  }
  int  m_bgColour;
  int  m_fgColour;
private:
  int  m_motion;
  Vec2 m_dest;
};


class ListOverlay : public UiOverlay
{
public:
  ListOverlay( GameControl& game,
	       ListProvider* provider,
	       int x, int y,
	       int w, int h )
    : UiOverlay(game,Rect(x,y,x+w,y+h)),
      m_provider( provider ),
      m_first( 0 ),
      m_current( 0 ),
      m_vgap( 2 ),
      m_hgap( 2 ),
      m_offset(0,0),
      m_velocity(0,0),
      m_listarea( m_hgap, m_vgap, w-m_hgap, h-m_vgap )
  {
    Canvas *first = m_provider->provideItem( m_first, NULL );
    m_itemh = first->height();
    m_numvis = m_listarea.height() / (m_itemh + m_vgap) + 2;

    if ( m_numvis <= m_provider->countItems() ) {
      m_allowscroll = false;
    } else {
      m_allowscroll = true;
      m_listarea.tl.y += 20;
      m_listarea.br.y -= 20;
      m_numvis = m_listarea.height() / (m_itemh + m_vgap);
      addButton( "Back", Rect(0,0,w,20), &ListOverlay::doBack );
      addButton( "Forward", Rect(0,y-20,w,20), &ListOverlay::doForward );      
    }

    m_items = (Canvas**)calloc( sizeof(Canvas*),  m_numvis);
    m_items[0] = first;

    for ( int i=1; i<m_numvis; i++ ) {
      m_items[i] = m_provider->provideItem( i, NULL );
    }
    renderItems();
  }

  ~ListOverlay()
  {
    for ( int i=0; i<m_numvis; i++ ) {
      if ( m_items[0] ) {
	m_provider->releaseItem( m_items[0] );
      }
    }
  }

  void renderItems()
  {
    m_canvas->clear( m_listarea );
    for ( int i=0; i < m_numvis; i++ ) {
      int q = m_first + i;
      if ( q < m_provider->countItems() ) {
	m_canvas->drawImage( m_items[q%m_numvis], 
			    m_listarea.tl.x + m_offset.x,
			    m_listarea.tl.y + m_offset.y + (m_itemh+m_vgap)*i );
      }
      if ( q == m_current ) {
	m_canvas->drawRect( m_listarea.tl.x-1 + m_offset.x,
			    m_listarea.tl.y + (m_itemh+m_vgap)*i - 1 + m_offset.y,
			    m_items[q%m_numvis]->width()+2,
			    m_items[q%m_numvis]->height()+2,
			    m_fgColour,
			    false );
      }
    }
    dirty();
  }

  void onTick( int tick )
  {
    UiOverlay::onTick( tick );
    if ( m_velocity.y ) {
      m_offset += m_velocity;
      while ( m_offset.y < -m_itemh-m_vgap ) {
	m_offset.y += m_itemh+m_vgap;
	doForward();
      }
      while ( m_offset.y >= m_itemh+m_vgap ) {
	m_offset.y -= m_itemh+m_vgap;
	doBack();
      }
      if ( m_velocity.y > 0 ) m_velocity.y--;
      if ( m_velocity.y < 0 ) m_velocity.y++;
      m_offset.x = 0;
      renderItems();
    }
  }

  virtual bool onClick( int x, int y ) 
  {
    int i = m_first + (y - m_vgap) / (m_itemh + m_vgap);
    if ( i >= 0 && i < m_provider->countItems() ) {
      m_current = i;
      m_provider->onSelection( m_current,
			       x - m_listarea.tl.x,
			       y - m_listarea.tl.y - (m_itemh+m_vgap)*i );
      renderItems();
    }
  }

  virtual bool onKey( int k )
  {
    switch (k) {
    case SDLK_UP:
      if ( m_current > 0 ) {
	m_current--;
	if ( m_current < m_first ) {
	  doBack();
	}
	renderItems();
      }
      return true;
    case SDLK_DOWN:
      if ( m_current < m_provider->countItems()-1 ) {
	m_current++;
	if ( m_current >= m_first+m_numvis-1 ) {
	  doForward();
	}
	renderItems();
      }
      return true;
    }
    return false;
  }

  bool onDrag( int dx, int dy )
  {
    m_velocity.x = dx;
    m_velocity.y = dy;
  }

  void doBack()
  {
    if ( m_first > 0 ) {
      m_first--;      
      m_items[m_first%m_numvis] = m_provider->provideItem( m_first,
							   m_items[m_first%m_numvis] );
    }
  }

  void doForward()
  {
    int last = m_first + m_numvis - 1;
    if ( last < m_provider->countItems() ) {
      m_first++;
      last = m_first + m_numvis - 1;
      m_items[last%m_numvis] = m_provider->provideItem( last, m_items[last%m_numvis] );
    }
  }

private:
  ListProvider *m_provider;
  int          m_first;
  int          m_current;
  int          m_numvis;
  int          m_itemh;
  int          m_hgap, m_vgap;
  bool         m_allowscroll;
  Rect         m_listarea;
  Vec2         m_offset;
  Vec2         m_velocity;
  Canvas**     m_items;
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

class NextLevelOverlay : public UiOverlay
{
public:
  NextLevelOverlay( GameControl& game )
    : UiOverlay(game,Rect(FULLSCREEN_RECT.centroid().x-220,
			  FULLSCREEN_RECT.centroid().y-120,
			  FULLSCREEN_RECT.centroid().x+220,
			  FULLSCREEN_RECT.centroid().y+160 ) ),
      m_levelIcon(-2),
      m_icon(NULL)
  {
    const Font *f = Font::titleFont();
    f->drawCenter( m_canvas, Vec2(220,7), "BRAVO!", 0 );
    f = Font::headingFont();
    f->drawLeft( m_canvas, Vec2(20,50),  "Stats:", 0 );
    f->drawLeft( m_canvas, Vec2(240,50), "Next:", 0 );

    addButton( "<<", Rect(230,190,270,210),
	       &NextLevelOverlay::doPrevLevel );
    addButton( ">>", Rect(380,190,420,210),
	       &NextLevelOverlay::doNextLevel );
    addButton( "Review", Rect(0,220,200,270),
	       &NextLevelOverlay::doActionReplay );
    addButton( "Continue", Rect(200,220,400,270),
	       &NextLevelOverlay::doContinue );
  }
  ~NextLevelOverlay()
  {
    delete m_icon;
  }
  virtual void onShow()
  {
    UiOverlay::onShow();
    m_game.m_refresh = true; //for fullscreen fade
    m_game.m_fade = true;
    m_selectedLevel = m_game.m_level+1;

    m_canvas->clear(Rect(20,80,220,210));
    char buf[80];
    const GameStats& stats = m_game.stats();
    const Font *f = Font::blurbFont();    
    f->drawLeft( m_canvas, Vec2(40,90),  "time", 0 );
    sprintf(buf,"%.02f", (float)(stats.endTime - stats.startTime)/1000.0f );
    f->drawRight( m_canvas, Vec2(160,90),  buf, 0 );
    f->drawLeft( m_canvas, Vec2(40,110), "strokes", 0 );
    sprintf(buf,"%d", stats.strokeCount );
    f->drawRight( m_canvas, Vec2(160,110),  buf, 0 );
    f->drawLeft( m_canvas, Vec2(50,130), "undone", 0 );
    sprintf(buf,"%d", stats.undoCount );
    f->drawRight( m_canvas, Vec2(160,130),  buf, 0 );
    f->drawLeft( m_canvas, Vec2(50,150), "paused", 0 );
    sprintf(buf,"%d", stats.pausedStrokes );
    f->drawRight( m_canvas, Vec2(160,150),  buf, 0 );
  }
  virtual void onHide()
  {
    UiOverlay::onHide();
    m_game.m_refresh = true; //for fullscreen fade
    m_game.m_fade = false;
  }
  virtual void draw( Canvas& screen, const Rect& area )
  {
    UiOverlay::draw( screen, area );
    if ( genIcon() ) {      
      Font::blurbFont()->drawCenter( &screen, Vec2(m_x+330,m_y+210), 
				       m_game.levels().levelName(m_selectedLevel), 0 );
      screen.drawImage( m_icon, m_x+230, m_y+85 );
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

      //Worker w( NULL, 1, 100, 200 );

      delete m_icon;
      m_icon = NULL;
      if ( m_selectedLevel < m_game.levels().numLevels() ) {
	Canvas temp( SCREEN_WIDTH, SCREEN_HEIGHT );
	if ( m_game.renderScene( temp, m_selectedLevel) ) {
	  m_icon = temp.scale( ICON_SCALE_FACTOR );
	}
      } else {
	m_icon = new Image("theend.png");
	m_caption = "no more levels!";
	m_selectedLevel = m_game.levels().numLevels();
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
  virtual void draw( Canvas& screen, const Rect& area )
  {
    IconOverlay::draw( screen, area );
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

Overlay* createListOverlay( GameControl& game,
			    ListProvider* provider,
			    int x, int y,
			    int w, int h )
{
  return new ListOverlay( game, provider, x, y, w, h );
}

Overlay* createMenuOverlay( GameControl& game )
{
  return new MenuOverlay( game );
}


