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

#include "Common.h"
#include "Array.h"
#include "Config.h"
#include "Game.h"
#include "Overlay.h"
#include "Path.h"
#include "Canvas.h"
#include "Font.h"
#include "Levels.h"
#include "Http.h"
#include "Os.h"
#include "Scene.h"

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>

using namespace std;

unsigned char levelbuf[64*1024];





struct DemoEntry {
  DemoEntry( int _t, int _o, SDL_Event& _e ) : t(_t), o(_o), e(_e) {}
  int t,o;
  SDL_Event e;
};

class DemoLog : public Array<DemoEntry>
{
public:
  std::string asString( int i )
  {
    if ( i < size() ) {
      DemoEntry& e = at(i);
      stringstream s;
      s << "E:" << e.t << " ";
      switch( e.e.type ) {
      case SDL_KEYDOWN:
	s << "K" << e.e.key.keysym.sym;
	break;
      case SDL_KEYUP:
	s << "k" << e.e.key.keysym.sym;
	break;
      case SDL_MOUSEBUTTONDOWN:
	s << "B" << '0'+e.e.button.button;
	s << "," << e.e.button.x << "," << e.e.button.y;
	break;
      case SDL_MOUSEBUTTONUP:
	s << "b" << '0'+e.e.button.button;
	s << "," << e.e.button.x << "," << e.e.button.y;
	break;
      case SDL_MOUSEMOTION:
	s << "M" << e.e.button.x << "," << e.e.button.y;
	break;
      default:
	return std::string();
      }
      return s.str();
    }
    return std::string();
  }

  void append( int tick, int offset, SDL_Event& ev ) 
  {
    Array<DemoEntry>::append( DemoEntry( tick, offset, ev ) );
  }

  void appendFromString( const std::string& str ) 
  {
    const char *s = str.c_str();
    int t, o, v1, v2, v3;
    char c;
    SDL_Event ev = {0};
    ev.type = 0xff;
    if ( sscanf(s,"E:%d %c%d",&t,&c,&v1)==3 ) { //1 arg
      switch ( c ) {
      case 'K': ev.type = SDL_KEYDOWN; break;
      case 'k': ev.type = SDL_KEYUP; break;
      }
      ev.key.keysym.sym = (SDLKey)v1;
    } else if ( sscanf(s,"E:%d %c%d,%d",&t,&c,&v1,&v2)==4 ) { //2 args
      switch ( c ) {
      case 'M': ev.type = SDL_MOUSEMOTION; break;
      }
      ev.button.x = v1;
      ev.button.y = v2;
    } else if ( sscanf(s,"E:%d %c%d,%d,%d",&t,&c,&v1,&v2,&v3)==5 ) { //3 args
      switch ( c ) {
      case 'B': ev.type = SDL_MOUSEBUTTONDOWN; break;
      case 'b': ev.type = SDL_MOUSEBUTTONUP; break;
      }
      ev.button.button = v1;
      ev.button.x = v2;
      ev.button.y = v3;
    }
    if ( ev.type != 0xff ) {
      append( t, o, ev );
    }
  }
};

class DemoRecorder
{
public:

  void start() 
  {
    m_running = true;
    m_log.empty();
    m_log.capacity(512);
    m_lastTick = 0;
    m_lastTickTime = SDL_GetTicks();
    m_pressed = 0;
  }

  void stop()  
  { 
    printf("stop recording: %d events:\n", m_log.size());
    for ( int i=0; i<m_log.size(); i++ ) {
      std::string e = m_log.asString(i);
      if ( e.length() > 0 ) {
	printf("  %s\n",e.c_str());
      }
    }
    m_running = false; 
  }

  void tick() 
  {
    if ( m_running ) {
      m_lastTick++;
      m_lastTickTime = SDL_GetTicks();
    }
  }

  void record( SDL_Event& ev )
  {
    if ( m_running ) {
      switch( ev.type ) {      
      case SDL_MOUSEBUTTONDOWN: 
	m_pressed |= 1<<ev.button.button;
	break;
      case SDL_MOUSEBUTTONUP: 
	m_pressed &= ~(1<<ev.button.button);
	break;
      case SDL_MOUSEMOTION:
	if ( m_pressed == 0 ) {
	  return;
	}
      }
      m_log.append( m_lastTick, SDL_GetTicks()-m_lastTickTime, ev );
    }
  }
  
  DemoLog& getLog() { return m_log; }

private:
  bool          m_running;
  DemoLog       m_log;
  int 		m_lastTick;
  int 		m_lastTickTime;
  int           m_pressed;
};


class DemoPlayer
{
public:

  void start( const DemoLog* log ) 
  {
    m_playing = true;
    m_log = log;
    m_index = 0;
    m_lastTick = 0;
    printf("start playback: %d events\n",m_log->size());
  }

  bool isRunning() { return m_playing; }

  void stop()  
  { 
    m_playing = false; 
    m_log = NULL;
  }

  void tick() 
  {
    if ( m_playing ) {
      m_lastTick++;
    }
  }

  bool fetchEvent( SDL_Event& ev )
  {
    if ( m_playing ) {
      if ( m_index < m_log->size()
	   && m_log->at(m_index).t <= m_lastTick ) {
	printf("demo event at t=%d\n",m_lastTick);
	ev = m_log->at(m_index).e;
	m_index++;
	return true;
      }
    }
    return false;
  }
  
private:
  bool           m_playing;
  const DemoLog* m_log;
  int            m_index;
  int  		 m_lastTick;
};


class CollectionSelector : public ListProvider
{
  Overlay* m_list;
public:
  CollectionSelector( GameControl& game )
  {
    m_list = createListOverlay( game, this );
  }
  Overlay* overlay() { return m_list; }

  virtual int     countItems() {
    return 58;
  }
  virtual Canvas* provideItem( int i, Canvas* old ) {
    delete old;
    char buf[18];
    sprintf(buf,"%d. Item",i);
    Canvas* c = new Canvas( 100, 32 );
    c->setBackground(0xffffff);
    c->clear();
    Font::headingFont()->drawLeft( c, Vec2(3,3), buf, i<<3 );
    return c;
  }
  virtual void    releaseItem( Canvas* old ) {
    delete old;
  }
  virtual void    onSelection( int i, int ix, int iy ) { 
    printf("Selected: %d (%d,%d)\n",i,ix,iy);
  }
};


struct GameStats
{
  int startTime;
  int endTime;
  int strokeCount;
  int pausedStrokes;
  int undoCount;
  void reset() {
    startTime = SDL_GetTicks();
    strokeCount = 0;
    pausedStrokes = 0;
    undoCount = 0;
  }
};

class Game : public GameControl, public Widget
{
  Scene   	    m_scene;
  Stroke  	   *m_createStroke;
  Stroke           *m_moveStroke;
  Array<Overlay*>   m_overlays;
  Window            m_window;
  Overlay          *m_pauseOverlay;
  Overlay          *m_editOverlay;
  Overlay          *m_completedOverlay;
  //  DemoOverlay       m_demoOverlay;
  DemoRecorder      m_recorder;
  DemoPlayer        m_player;
  CollectionSelector m_cselector;
  Os               *m_os;
  GameStats         m_stats;
  bool              m_isCompleted;
  bool              m_paused;
public:
  Game( Levels* levels, int width, int height ) 
  : m_createStroke(NULL),
    m_moveStroke(NULL),
    m_window(width,height,"Numpty Physics","NPhysics"),
    m_pauseOverlay( NULL ),
    m_editOverlay( NULL ),
    m_completedOverlay( createNextLevelOverlay(*this) ),
    m_isCompleted(false),
    m_cselector( *this ),
    m_os( Os::get() ),
    m_paused( false )
    //,m_demoOverlay( *this )
  {
    configureScreenTransform( m_window.width(), m_window.height() );
    m_levels = levels;
    gotoLevel(0);
  }


  virtual bool renderScene( Canvas& c, int level ) 
  {
    Scene scene( true );
    int size = m_levels->load( level, levelbuf, sizeof(levelbuf) );
    if ( size && scene.load( levelbuf, size ) ) {
      scene.draw( c, FULLSCREEN_RECT );
      return true;
    }
    return false;
  }

  void gotoLevel( int level, bool replay=false )
  {
    if ( level >= 0 && level < m_levels->numLevels() ) {
      int size = m_levels->load( level, levelbuf, sizeof(levelbuf) );
      if ( size && m_scene.load( levelbuf, size ) ) {
	m_scene.activateAll();
	//m_window.setSubName( file );
	m_refresh = true;
	if ( m_edit ) {
	  m_scene.protect(0);
	}
	m_recorder.stop();
	m_player.stop();
	if ( replay ) {
	  m_player.start( &m_recorder.getLog() );
	} else {
	  m_recorder.start();
	}
	m_level = level;
	m_stats.reset();
      }
    }
  }


  bool save( const char *file=NULL )
  {	  
    string p;
    if ( file ) {
      p = file;
    } else {
      p = Config::userDataDir() + Os::pathSep + "L99_saved.nph";
    }
    if ( m_scene.save( p ) ) {
      m_levels->addPath( p.c_str() );
      int l = m_levels->findLevel( p.c_str() );
      if ( l >= 0 ) {
	m_level = l;
	m_window.setSubName( p.c_str() );
      }
      return true;
    }
    return false;
  }

  bool send()
  {
    if ( save( SEND_TEMP_FILE ) ) {
      Http h;
      if ( h.post( Config::planetRoot().c_str(),
		   "upload", SEND_TEMP_FILE, "type=level" ) ) {
	std::string id = h.getHeader("NP-Upload-Id");
	if ( id.length() > 0 ) {
	  printf("uploaded as id %s\n",id.c_str());
	  if ( !m_os->openBrowser((Config::planetRoot()+"?level="+id).c_str()) ) {
	    showMessage("Unable to launch browser");
	  }
	} else {
	  showMessage("UploadFailed: unknown error");
	}
      } else {
	showMessage(std::string("UploadFailed: ")+h.errorMessage());
      }
    }
    return false;
  }

  void setTool( int t )
  {
    m_colour = t;
  }

  void editMode( bool set )
  {
    m_edit = set;
  }

  void showMessage( const std::string& msg )
  {
    //todo
    printf("showMessage \"%s\"\n",msg.c_str());
  }

  void showOverlay( Overlay* o )
  {
    parent()->add( o );
    o->onShow();
  }

  void hideOverlay( Overlay* o )
  {
    parent()->remove( o );
    o->onHide();
    m_refresh = true;
  }

  void togglePause()
  {
    if ( !m_paused ) {
      if ( !m_pauseOverlay ) {
	m_pauseOverlay = createIconOverlay( *this, "pause.png", 50, 50 );
      }
      showOverlay( m_pauseOverlay );
      m_paused = true;
    } else {
      hideOverlay( m_pauseOverlay );
      m_paused = false;
    }
  }

  bool isPaused()
  {
    return m_paused;
  }

  void edit( bool doEdit )
  {
    if ( m_edit != doEdit ) {
      m_edit = doEdit;
      if ( m_edit ) {
	if ( !m_editOverlay ) {
	  m_editOverlay = createEditOverlay(*this);
	}
	showOverlay( m_editOverlay );
	m_scene.protect(0);
      } else {
	hideOverlay( m_editOverlay );
	m_strokeFixed = false;
	m_strokeSleep = false;
	m_strokeDecor = false;
	if ( m_colour < 2 ) m_colour = 2;
	m_scene.protect();
      }
    }
  }

  Vec2 mousePoint( SDL_Event ev )
  {
    Vec2 pt( ev.button.x, ev.button.y );
    worldToScreen.inverseTransform( pt );
    return pt;
  }

  bool handleGameEvent( SDL_Event &ev )
  {
    switch( ev.type ) {
    case SDL_KEYDOWN:
      switch ( ev.key.keysym.sym ) {
      case SDLK_SPACE:
      case SDLK_KP_ENTER:
      case SDLK_RETURN:
	togglePause();
	break;
      case SDLK_s:
	save();
	break;
      case SDLK_F4: 
	showOverlay( createMenuOverlay( *this ) );
	break;
      case SDLK_c:
	showOverlay( m_cselector.overlay() );
	break;
      case SDLK_e:
      case SDLK_F6:
	edit( !m_edit );
	break;
      case SDLK_d:
	//toggleOverlay( m_demoOverlay );
	break;
      case SDLK_r:
      case SDLK_UP:
	gotoLevel( m_level );
	break;
      case SDLK_n:
      case SDLK_RIGHT:
	gotoLevel( m_level+1 );
	break;
      case SDLK_p:
      case SDLK_LEFT:
	gotoLevel( m_level-1 );
	break;
      case SDLK_v:
	gotoLevel( m_level, true );
	break;
      default:
	break;
      }
      break;
    default:
      break;
    }
    return false;
  }


  bool handleModEvent( SDL_Event &ev )
  {
    static int mod=0;
    //printf("mod=%d\n",ev.key.keysym.sym,mod);
    switch( ev.type ) {      
    case SDL_KEYDOWN:
      //printf("mod key=%x mod=%d\n",ev.key.keysym.sym,mod);
      if ( ev.key.keysym.sym == SDLK_F8 ) {
	mod = 1;  //zoom- == middle (delete)
	return true;
      } else if ( ev.key.keysym.sym == SDLK_F7 ) {
	mod = 2;  //zoom+ == right (move)
	return true;
      }
      break;
    case SDL_KEYUP:
      if ( ev.key.keysym.sym == SDLK_F7
	   || ev.key.keysym.sym == SDLK_F8 ) {
	mod = 0;     
	return true;
      }
      break;
    case SDL_MOUSEBUTTONDOWN: 
    case SDL_MOUSEBUTTONUP: 
      if ( ev.button.button == SDL_BUTTON_LEFT && mod != 0 ) {
	ev.button.button = ((mod==1) ? SDL_BUTTON_MIDDLE : SDL_BUTTON_RIGHT);
      }
      break;
    }
    return false;
  }

  bool handlePlayEvent( SDL_Event &ev )
  {
    switch( ev.type ) {      
    case SDL_MOUSEBUTTONDOWN: 
      if ( ev.button.button == SDL_BUTTON_LEFT
	   && !m_createStroke ) {
	int attrib;
	if ( m_strokeFixed ) {
	  attrib |= ATTRIB_GROUND;
	}
	if ( m_strokeSleep ) {
	  attrib |= ATTRIB_SLEEPING;
	}
	if ( m_strokeDecor ) {
	  attrib |= ATTRIB_DECOR;
	}
	m_createStroke = m_scene.newStroke( Path()&mousePoint(ev), m_colour, attrib );
	return true;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if ( ev.button.button == SDL_BUTTON_LEFT
	   && m_createStroke ) {
	if ( m_scene.activate( m_createStroke ) ) {
	  m_stats.strokeCount++;
	  if ( isPaused() ) {
	    m_stats.pausedStrokes++; 
	  }
	} else {
	  m_scene.deleteStroke( m_createStroke );
	}
	m_createStroke = NULL;
	return true;
      }
      break;
    case SDL_MOUSEMOTION:
      if ( m_createStroke ) {
	m_scene.extendStroke( m_createStroke, mousePoint(ev) );
	return true;
      }
      break;
    case SDL_KEYDOWN:
      if ( ev.key.keysym.sym == SDLK_ESCAPE ) {
	if ( m_createStroke ) {
	  m_scene.deleteStroke( m_createStroke );
	  m_createStroke = NULL;
	} else if ( m_scene.deleteStroke( m_scene.strokes().at(m_scene.strokes().size()-1) ) ) {
	  m_stats.undoCount++;
	}
	m_refresh = true;
	return true;
      }
      break;
    default:
      break;
    }
    return false;
  }

  bool handleEditEvent( SDL_Event &ev )
  {
    //allow move/delete in normal play!!
    //if ( !m_edit ) return false;

    switch( ev.type ) {      
    case SDL_MOUSEBUTTONDOWN: 
      if ( ev.button.button == SDL_BUTTON_RIGHT
	   && !m_moveStroke ) {
	m_moveStroke = m_scene.strokeAtPoint( mousePoint(ev),
					      SELECT_TOLERANCE );
	return true;
      } else if ( ev.button.button == SDL_BUTTON_MIDDLE ) {
	m_scene.deleteStroke( m_scene.strokeAtPoint( mousePoint(ev),
						     SELECT_TOLERANCE ) );
	m_refresh = true;
	return true;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if ( ev.button.button == SDL_BUTTON_RIGHT
	   && m_moveStroke ) {
	m_moveStroke = NULL;
	return true;
      }
      break;
    case SDL_MOUSEMOTION:
      if ( m_moveStroke ) {
	m_scene.moveStroke( m_moveStroke, mousePoint(ev) );
	return true;
      }
      break;
    default:
      break;
    }
    return false;
  }

  ////////////////////////////////////////////////////////////////
  // layer interface
  ////////////////////////////////////////////////////////////////

  virtual bool isDirty()
  {
    //TODO this can be a bit heavyweight
    return !dirtyArea().isEmpty();
  }

  virtual Rect dirtyArea() 
  {
    if ( m_refresh  ) {
      m_refresh = false;
      return FULLSCREEN_RECT;
    } else {
      return m_scene.dirtyArea();
    }
  }

  virtual void onTick( int tick ) 
  {
    if ( !isPaused() ) {
      m_scene.step();
      m_recorder.tick();
      m_player.tick();
    }

    SDL_Event ev;
    while ( m_player.fetchEvent(ev) ) {
      // todo only dispatch play events?
      handleModEvent(ev)
	|| handleGameEvent(ev)
	|| handleEditEvent(ev)
	|| handlePlayEvent(ev);
    }

    if ( m_isCompleted && m_edit ) {
      hideOverlay( m_completedOverlay );
      m_isCompleted = false;
    }
    if ( m_scene.isCompleted() != m_isCompleted && !m_edit ) {
      m_isCompleted = m_scene.isCompleted();
      if ( m_isCompleted ) {
	m_stats.endTime = SDL_GetTicks();
	m_player.stop();
	m_recorder.stop();
	showOverlay( m_completedOverlay );
      } else {
	hideOverlay( m_completedOverlay );
      }
    }

    if ( m_os ) {
      m_os->poll();      
      for ( char *f = m_os->getLaunchFile(); f; f=m_os->getLaunchFile() ) {
	if ( strstr(f,".npz") ) {
	  //m_levels->empty();
	}
	m_levels->addPath( f );
	int l = m_levels->findLevel( f );
	if ( l >= 0 ) {
	  gotoLevel( l );
	  m_window.raise();
	}
      }    
    }  
  }

  virtual void draw( Canvas& screen, const Rect& area )
  {
    m_scene.draw( screen, area );
    if ( m_fade ) {
      screen.fade( area );
    }
  }

  virtual bool handleEvent( SDL_Event& ev )
  {
    if ( m_player.isRunning() 
	 && ( ev.type==SDL_MOUSEMOTION || 
	      ev.type==SDL_MOUSEBUTTONDOWN || 
	      ev.type==SDL_MOUSEBUTTONUP ) ) {
      return false;
    } else if (handleModEvent(ev)
	       || handleGameEvent(ev)
	       || handleEditEvent(ev)
	       || handlePlayEvent(ev)) {
      //\TODO only record edit,play events etc
      m_recorder.record( ev );
      return true;
    }
    return false;
  }
};



Widget* createGameLayer( Levels* levels, int width, int height )
{
  return new Game(levels,width,height);
}
