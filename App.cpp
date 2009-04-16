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
  bool  m_quit;
  Array<const char*> m_files;
  Array<Widget *>    m_layers;
  Window            *m_window;
public:
  App(int argc, char** argv)
    : m_width(SCREEN_WIDTH),
      m_height(SCREEN_HEIGHT),
      m_rotate(false),
      m_thumbnailMode(false),
      m_quit(false),
      m_window(NULL)
  {
    for ( int i=1; i<argc; i++ ) {
      if ( strcmp(argv[i],"-bmp")==0 ) {
	m_thumbnailMode = true;
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
    } else {      
      m_window = new Window(m_width,m_height,"Numpty Physics","NPhysics"),
      runGame( m_files, m_width, m_height );
    }
  }

private:

  void init()
  {
    if ( m_thumbnailMode ) {
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
      Rect dirt = m_layers[i]->dirtyArea();
      if ( !dirt.isEmpty() ) {
	if ( area.isEmpty() ) {
	  area = dirt;
	} else { 
	  area.expand( dirt );
	}
      }
    }
    for ( int i=0; i<m_layers.size(); i++ ) {
      m_layers[i]->draw( *m_window, area );
    }
    m_window->drawRect( area, 0x00ff00, false );
    m_window->update( area );
  }


  bool handleGameEvent( SDL_Event &ev )
  {
    switch( ev.type ) {
    case SDL_QUIT:
      m_quit = true;
      return true;
    case SDL_KEYDOWN:
      if ( ev.key.keysym.sym == SDLK_q ) {
	m_quit = true;
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

    int renderRate = (MIN_RENDER_RATE+MAX_RENDER_RATE)/2;
    int iterationRate = ITERATION_RATE;
    int iterateCounter = 0;
    int lastTick = SDL_GetTicks();
    bool isComplete = false;    

    while ( !m_quit ) {

      //assumes RENDER_RATE <= ITERATION_RATE
      //TODO dynamic tick scaling for improved sleep
      while ( iterateCounter < iterationRate ) {
	dispatchEvents( lastTick );
	if ( m_quit ) return;
	iterateCounter += renderRate;
      }
      iterateCounter -= iterationRate;

      renderLayers();

      int sleepMs = lastTick + 1000/renderRate -  SDL_GetTicks();

      if ( sleepMs > 1 && renderRate < MAX_RENDER_RATE ) {
	renderRate++;
	printf("increasing render rate to %dfps\n",renderRate);
	sleepMs = lastTick + 1000/renderRate -  SDL_GetTicks();
      }

      if ( sleepMs > 0 ) {
	SDL_Delay( sleepMs );
      } else {
	printf("overrun %dms\n",-sleepMs);
	if ( renderRate > MIN_RENDER_RATE ) {
	  renderRate--;
	  printf("decreasing render rate to %dfps\n",renderRate);
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
