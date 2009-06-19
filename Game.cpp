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
#include "Script.h"

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


#define JOINT_IND_PATH "282,39 280,38 282,38 285,39 300,39 301,60 303,66 302,64 301,63 300,48 297,41 296,42 294,43 293,45 291,46 289,48 287,49 286,52 284,53 283,58 281,62 280,66 282,78 284,82 287,84 290,85 294,88 297,88 299,89 302,90 308,90 311,89 314,89 320,85 321,83 323,83 324,81 327,78 328,75 327,63 326,58 325,55 323,54 321,51 320,49 319,48 316,46 314,44 312,43 314,43"




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
  CollectionSelector m_cselector;
  Os               *m_os;
  bool              m_isCompleted;
  bool              m_paused;
  Path              m_jointCandidates;
  Path              m_jointInd;
  Canvas           *m_spot;
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
    m_paused( false ),
    m_jointInd(JOINT_IND_PATH),
    m_spot(new Image("spot.png",true))
    //,m_demoOverlay( *this )
  {
    m_jointInd.scale( 12.0f / (float32)m_jointInd.bbox().width() );
    //m_jointInd.simplify( 2.0f );
    m_jointInd.makeRelative();
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
    bool ok = false;

    if ( replay ) {
      // reset scene, delete user strokes, but retain log
      m_scene.reset( NULL, true );
      m_scene.start( true );
      ok = true;
    } else if ( level >= 0 && level < m_levels->numLevels() ) {
      int size = m_levels->load( level, levelbuf, sizeof(levelbuf) );
      if ( size && m_scene.load( levelbuf, size ) ) {
	m_scene.start( m_scene.getLog()->size() > 0 );
	ok = true;
      }
    }

    if (ok) {
      //m_window.setSubName( file );
      if ( m_edit ) {
	m_scene.protect(0);
      }
      m_refresh = true;
      m_level = level;
      m_stats.reset(SDL_GetTicks());
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
      if ( h.post( (Config::planetRoot()+"/upload").c_str(),
		   "data", SEND_TEMP_FILE, "type=1" ) ) {
	std::string id = h.getHeader("NP-Upload-Id");
	if ( id.length() > 0 ) {
	  printf("uploaded as id %s\n",id.c_str());
	  if ( !m_os->openBrowser((Config::planetRoot()+"/editlevel?id="+id).c_str()) ) {
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
	m_scene.save("test.npd",true);
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


  //TODO move this to Os event filter
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
	if ( m_scene.activateStroke( m_createStroke ) ) {
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
    //todo include dirt  for old joint candidates
    m_jointCandidates.empty();
    if ( m_refresh  ) {
      m_refresh = false;
      if ( m_createStroke ) {
	//this messes up dirty calc so do _after_ dirty area eval.
	m_scene.getJointCandidates( m_createStroke, m_jointCandidates );
      }
      return FULLSCREEN_RECT;
    } else {
      Rect r = m_scene.dirtyArea();
      if ( m_createStroke ) {
	//this messes up dirty calc so do _after_ dirty area eval.
	m_scene.getJointCandidates( m_createStroke, m_jointCandidates );
	if ( m_jointCandidates.size() ) {
	  Rect jr = m_jointCandidates.bbox();
	  jr.grow( 8 );
	  r.expand( jr );
	}
      }
      r.grow(8);
      return r;
    }
  }

  virtual void onTick( int tick ) 
  {
    m_scene.step( isPaused() );

    if ( m_isCompleted && m_edit ) {
      hideOverlay( m_completedOverlay );
      m_isCompleted = false;
    }
    if ( m_scene.isCompleted() != m_isCompleted && !m_edit ) {
      m_isCompleted = m_scene.isCompleted();
      if ( m_isCompleted ) {
	m_stats.endTime = SDL_GetTicks();
	fprintf(stderr,"STATS:\ttime=%dms\n\t"
		"strokes=%d (%d paused, %d undone)\n",
		m_stats.endTime-m_stats.startTime, m_stats.strokeCount,
		m_stats.pausedStrokes, m_stats.undoCount);
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
    static int drawCount = 0 ;
    m_scene.draw( screen, area );
    if ( m_jointCandidates.size() ) {
      float32 rot = (float32)(drawCount&127) / 128.0f;
      for ( int i=0; i<m_jointCandidates.size(); i++ ) {
	Rect r( m_jointCandidates[i], m_jointCandidates[i] );
	r.grow( 5 );
	Path joint = m_jointInd;
	joint.rotate( b2Mat22(rot*2.0*3.141) );
	joint.translate( r.tl - joint.bbox().tl );
	screen.drawPath( joint, 0x606060 );
      }
      drawCount++;
    }
    if ( m_fade ) {
      screen.fade( area );
    }
  }

  virtual bool handleEvent( SDL_Event& ev )
  {
    bool isReplay = m_scene.replay()->isRunning();

    if (handleModEvent(ev)
	|| handleGameEvent(ev)
	|| (!isReplay && (handleEditEvent(ev)
			  || handlePlayEvent(ev)))) {
      return true;
    }
    return false;
  }
};



Widget* createGameLayer( Levels* levels, int width, int height )
{
  return new Game(levels,width,height);
}
