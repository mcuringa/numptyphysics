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
#include "Scene.h"
#include "Levels.h"
#include "Canvas.h"
#include "Ui.h"
#include "Font.h"

#include <cstdio>
#include <string>
#include <sys/stat.h>
#include <SDL/SDL.h>


class App : private WidgetParent
{
  int   m_width;
  int   m_height;
  bool  m_rotate;
  bool  m_thumbnailMode;
  bool  m_videoMode;
  bool  m_quit;
  bool  m_drawFps;
  bool  m_drawDirty;
  int   m_renderRate;
  Array<const char*> m_files;
  Array<Widget *>    m_layers;
  Window            *m_window;
public:
  App(int argc, char** argv)
    : m_width(SCREEN_WIDTH),
      m_height(SCREEN_HEIGHT),
      m_rotate(false),
      m_thumbnailMode(false),
      m_videoMode(false),
      m_quit(false),
      m_drawFps(false),
      m_drawDirty(false),
      m_window(NULL)
  {
    for ( int i=1; i<argc; i++ ) {
      if ( strcmp(argv[i],"-bmp")==0 ) {
	m_thumbnailMode = true;
      } else if ( strcmp(argv[i],"-video")==0 ) {
	m_videoMode = true;
      } else if ( strcmp(argv[i],"-fps")==0 ) {
	m_drawFps = true;
      } else if ( strcmp(argv[i],"-rotate")==0 ) {
	m_rotate = true;
      } else if ( strcmp(argv[i],"-geometry")==0 && i<argc-1) {
	int ww, hh;
	if ( sscanf(argv[i+1],"%dx%d",&ww,&hh) ==2 ) {
	  m_width = ww; 
	  m_height = hh;
	}
	i++;
      } else {
	m_files.append( argv[i] );
      }
    }
    init();
  }

  ~App()
  {
    delete m_window;
  }

  void run()
  {
    if ( m_thumbnailMode ) {
      for ( int i=0; i<m_files.size(); i++ ) {
	renderThumbnail( m_files[i], m_width, m_height );
      }
    } else if ( m_videoMode ) {
      for ( int i=0; i<m_files.size(); i++ ) {
	renderVideo( m_files[i], m_width, m_height );
      }
    } else {      
      m_window = new Window(m_width,m_height,"Numpty Physics","NPhysics"),
      runGame( m_files, m_width, m_height );
    }
  }

private:

  void init()
  {
    if ( m_thumbnailMode || m_videoMode ) {
      putenv((char*)"SDL_VIDEODRIVER=dummy");
    } else {
      putenv((char*)"SDL_VIDEO_X11_WMCLASS=NPhysics");

      if ( mkdir( (std::string(getenv("HOME"))+"/"USER_BASE_PATH).c_str(),
		  0755)!=0 ) {
	fprintf(stderr,"failed to create user dir\n");
      } 
    }
    
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
      throw "Couldn't initialize SDL";
    }   
  }


  void renderThumbnail( const char* file, int width, int height )
  {
    configureScreenTransform( width, height );
    Scene scene( true );
    if ( scene.load( file ) ) {
      printf("generating bmp %s\n", file);
      Canvas temp( width, height );
      scene.draw( temp, FULLSCREEN_RECT );
      std::string bmp( file );
      bmp += ".bmp";
      temp.writeBMP( bmp.c_str() );
    }
  }


  void renderVideo( const char* file, int width, int height )
  {
    configureScreenTransform( width, height );

    Levels* levels = new Levels();
    levels->addPath( file );
    add( createGameLayer( levels, width, height ) );

    Rect area(0,0,width,height);
    Canvas canvas(width,height);
    int iterateCounter = 0;

    for ( int f=0; f<VIDEO_FPS*VIDEO_MAX_LEN; f++ ) {
      while ( iterateCounter < ITERATION_RATE ) {
	m_layers[0]->onTick( f*1000/VIDEO_FPS );
	iterateCounter += VIDEO_FPS;
      }
      iterateCounter -= ITERATION_RATE;
      m_layers[0]->draw( canvas, area );
      char bfile[128];
      sprintf(bfile,"%s.%04d.bmp",file,f);
      canvas.writeBMP( bfile );
    }
  }

  void add( Widget* w )
  {
    m_layers.append( w );
    w->setParent( this );
  }

  void remove( Widget* w )
  {
    if ( m_layers.indexOf( w ) >= 0 ) {
      m_layers.erase( m_layers.indexOf( w ) );
      w->setParent( NULL );
    }
  }


  void runGame( Array<const char*>& files, int width, int height )
  {
    Levels* levels = new Levels();
    
    if ( files.size() > 0 ) {
      for ( int i=0; i<files.size(); i++ ) {
	levels->addPath( files[i] );
      }
    } else {
      struct stat st;
      if ( stat("Game.cpp",&st)==0 ) {
	levels->addPath( "data" );
      } else {
	levels->addPath( DEFAULT_LEVEL_PATH );
      }
      levels->addPath( Config::userDataDir().c_str() );
    }
        
    add( createGameLayer( levels, width, height ) );
    mainLoop();
  }

  void renderLayers()
  {
    Rect area = m_layers[0]->dirtyArea();

    for ( int i=1; i<m_layers.size(); i++ ) {
      area.expand( m_layers[i]->dirtyArea() );
    }

    for ( int i=0; i<m_layers.size(); i++ ) {
      m_layers[i]->draw( *m_window, area );
    }
 
    if ( m_drawDirty ) {
      m_window->drawRect( area, 0x00ff00, false );
    }

    if ( m_drawFps ) {
      m_window->drawRect( Rect(0,0,50,50), 0xbfbf8f, true );
      char buf[32];
      sprintf(buf,"%d",m_renderRate);
      Font::headingFont()->drawLeft( m_window, Vec2(20,20), buf, 0 );
      area.expand( Rect(0,0,50,50) );
    }

    m_window->update( area );
  }


  bool handleGameEvent( SDL_Event &ev )
  {
    switch( ev.type ) {
    case SDL_QUIT:
      m_quit = true;
      return true;
    case SDL_KEYDOWN:
      switch ( ev.key.keysym.sym ) {
      case SDLK_q:
	m_quit = true;
	return true;
      case SDLK_1:
	m_drawFps = !m_drawFps;
	return true;
      case SDLK_2:
	m_drawDirty = !m_drawDirty;
	return true;
      }
    }
    return false;
  }

  void dispatchEvents( int lastTick )
  {
    for ( int i=0; i<m_layers.size(); i++ ) {
      m_layers[i]->onTick( lastTick );
    }
    
    SDL_Event ev;
    while ( SDL_PollEvent(&ev) ) {
      if ( !handleGameEvent(ev) ) {
	for ( int i=m_layers.size()-1; i>=0; i-- ) {
	  if ( m_layers[i]->handleEvent(ev) ) {
	    break;
	  }
	}
      }
    }
  }

  void mainLoop()
  {
    renderLayers();

    m_renderRate = (MIN_RENDER_RATE+MAX_RENDER_RATE)/2;
    int iterationRate = ITERATION_RATE;
    int iterateCounter = 0;
    int lastTick = SDL_GetTicks();
    bool isComplete = false;    

    while ( !m_quit ) {

      //assumes RENDER_RATE <= ITERATION_RATE
      while ( iterateCounter < iterationRate ) {
	dispatchEvents( lastTick );
	if ( m_quit ) return;
	iterateCounter += m_renderRate;
      }
      iterateCounter -= iterationRate;

      renderLayers();

      int sleepMs = lastTick + 1000/m_renderRate -  SDL_GetTicks();

      if ( sleepMs > 1 && m_renderRate < MAX_RENDER_RATE ) {
	m_renderRate++;
	//printf("increasing render rate to %dfps\n",m_renderRate);
	sleepMs = lastTick + 1000/m_renderRate -  SDL_GetTicks();
      }

      if ( sleepMs > 0 ) {
	SDL_Delay( sleepMs );
      } else {
	//printf("overrun %dms\n",-sleepMs);
	if ( m_renderRate > MIN_RENDER_RATE ) {
	  m_renderRate--;
	  //printf("decreasing render rate to %dfps\n",m_renderRate);
	} else if ( iterationRate > 30 ) {
	  //slow down simulation time to maintain fps??
	}
      }
      lastTick = SDL_GetTicks();
    }
  }
  



};


int npmain(int argc, char** argv)
{
  try {      
    App app(argc,argv);
    app.run();
  } catch ( const char* e ) {
    fprintf(stderr,"*** CAUGHT: %s",e);
  } 
  return 0;
}
