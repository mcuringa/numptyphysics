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

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <cstdio>
#include <iostream>
#include <sstream>
#include <fstream>
#include <memory.h>
#include <errno.h>
#include <sys/stat.h>

#include "Common.h"
#include "Array.h"
#include "Config.h"
#include "Path.h"
#include "Canvas.h"
#include "Levels.h"
#ifdef USE_HILDON
# include "Hildon.h"
#endif //USE_HILDON

using namespace std;

const Rect FULLSCREEN_RECT( 0, 0, CANVAS_WIDTH-1, CANVAS_HEIGHT-1 );
const Rect BOUNDS_RECT( -CANVAS_WIDTH/4, -CANVAS_HEIGHT,
			CANVAS_WIDTH*5/4, CANVAS_HEIGHT );

const int brush_colours[] = {
  0xb80000, //red
  0xeec900, //yellow
  0x000077, //blue
  0x108710, //green
  0x101010, //black
  0x8b4513, //brown
  0x87cefa, //lightblue
  0xee6aa7, //pink
  0xb23aee, //purple
  0x00fa9a, //lightgreen
  0xff7f00, //orange
  0x6c7b8b, //grey
};
#define NUM_COLOURS (sizeof(brush_colours)/sizeof(brush_colours[0]))

class Stroke
{
public:
  typedef enum {
    ATTRIB_DUMMY = 0,
    ATTRIB_GROUND = 1,
    ATTRIB_TOKEN = 2,
    ATTRIB_GOAL = 4,
    ATTRIB_DECOR = 8,
    ATTRIB_SLEEPING = 16,
    ATTRIB_CLASSBITS = ATTRIB_TOKEN | ATTRIB_GOAL
  } Attribute;

private:
  struct JointDef : public b2RevoluteJointDef
  {
    JointDef( b2Body* b1, b2Body* b2, const b2Vec2& pt )
    {
      body1 = b1;
      body2 = b2;
      anchorPoint = pt;
      motorTorque = 20.0f;
      motorSpeed = 0.0f;
      enableMotor = true;
    }
  };

  struct BoxDef : public b2BoxDef
  {
    void init( const Vec2& p1, const Vec2& p2, int attr )
    {
      b2Vec2 barOrigin = p1;
      b2Vec2 bar = p2 - p1;
      bar *= 1.0f/PIXELS_PER_METREf;
      barOrigin *= 1.0f/PIXELS_PER_METREf;;
      extents.Set( bar.Length()/2.0f, 0.1f );
      localPosition = 0.5f*bar + barOrigin;
      localRotation = vec2Angle( bar );
      if ( attr & ATTRIB_GROUND ) {
	  density = 0.0f;
      } else if ( attr & ATTRIB_GOAL ) {
	density = 100.0f;
      } else if ( attr & ATTRIB_TOKEN ) {
	density = 3.0f;
      } else {
	density = 5.0f;
      }
      friction = 0.3f;
      restitution = 0.2f;
    }
  };

public:
  Stroke( const Path& path )
    : m_rawPath(path)
  {
    m_colour = COLOUR_BLUE;
    m_attributes = 0;
    m_origin = m_rawPath.point(0);
    m_rawPath.translate( -m_origin );
    reset();
  }  

  Stroke( const string& str ) 
  {
    int col = 0;
    m_colour = brush_colours[2];
    m_attributes = 0;
    m_origin = Vec2(400,240);
    reset();
    const char *s = str.c_str();
    while ( *s && *s!=':' && *s!='\n' ) {
      switch ( *s ) {
      case 't': setAttribute( ATTRIB_TOKEN ); break;	
      case 'g': setAttribute( ATTRIB_GOAL ); break;	
      case 'f': setAttribute( ATTRIB_GROUND ); break;
      case 's': setAttribute( ATTRIB_SLEEPING ); break;
      case 'd': setAttribute( ATTRIB_DECOR ); break;
      default:
	if ( *s >= '0' && *s <= '9' ) {
	  col = col*10 + *s -'0';
	}
	break;
      }
      s++;
    }
    if ( col >= 0 && col < NUM_COLOURS ) {
      m_colour = brush_colours[col];
    }
    if ( *s++ == ':' ) {
      float x,y;      
      while ( sscanf( s, "%f,%f", &x, &y )==2) {
	m_rawPath.append( Vec2((int)x,(int)y) );
	while ( *s && *s!=' ' && *s!='\t' ) s++;
	while ( *s==' ' || *s=='\t' ) s++;
      }
    }
    if ( m_rawPath.size() < 2 ) {
      throw "invalid stroke def";
    }
    m_origin = m_rawPath.point(0);
    m_rawPath.translate( -m_origin );
    setAttribute( ATTRIB_DUMMY );
  }

  void reset( b2World* world=NULL )
  {
    if (m_body && world) {
      world->DestroyBody( m_body );
    }
    m_body = NULL;
    m_xformAngle = 7.0f;
    m_drawnBbox.tl = m_origin;
    m_drawnBbox.br = m_origin;
    m_jointed[0] = m_jointed[1] = false;
    m_shapePath = m_rawPath;
    m_hide = 0;
    m_drawn = false;
  }

  string asString()
  {
    stringstream s;
    s << 'S';
    if ( hasAttribute(ATTRIB_TOKEN) )    s<<'t';
    if ( hasAttribute(ATTRIB_GOAL) )     s<<'g';
    if ( hasAttribute(ATTRIB_GROUND) )   s<<'f';
    if ( hasAttribute(ATTRIB_SLEEPING) ) s<<'s';
    if ( hasAttribute(ATTRIB_DECOR) )    s<<'d';
    for ( int i=0; i<NUM_COLOURS; i++ ) {
      if ( m_colour==brush_colours[i] )  s<<i;
    }
    s << ":";
    for ( int i=0; i<m_rawPath.size(); i++ ) {
      Vec2 p = m_rawPath.point(i);
      p += m_origin;
      s <<' '<< p.x << ',' << p.y; 
    }
    s << endl;
    return s.str();
  }

  void setAttribute( Stroke::Attribute a )
  {
    m_attributes |= a;
    if ( m_attributes & ATTRIB_TOKEN )     m_colour = COLOUR_RED;
    else if ( m_attributes & ATTRIB_GOAL ) m_colour = COLOUR_YELLOW;
  }

  bool hasAttribute( Stroke::Attribute a )
  {
    return (m_attributes&a) != 0;
  }
  void setColour( int c ) 
  {
    m_colour = c;
  }

  void createBodies( b2World& world )
  {
    process();
    if ( hasAttribute( ATTRIB_DECOR ) ){
      return; //decorators have no physical embodiment
    }
    int n = m_shapePath.numPoints();
    if ( n > 1 ) {
      BoxDef boxDef[n];
      b2BodyDef bodyDef;
      for ( int i=1; i<n; i++ ) {
	boxDef[i].init( m_shapePath.point(i-1),
			m_shapePath.point(i),
			m_attributes );
	bodyDef.AddShape( &boxDef[i] );
      }
      bodyDef.position = m_origin;
      bodyDef.position *= 1.0f/PIXELS_PER_METREf;
      bodyDef.userData = this;
      if ( m_attributes & ATTRIB_SLEEPING ) {
	bodyDef.isSleeping = true;
      }
      m_body = world.CreateBody( &bodyDef );
    }
    transform();
  }

  bool maybeCreateJoint( b2World& world, Stroke* other )
  {
    if ( (m_attributes&ATTRIB_CLASSBITS)
	 != (other->m_attributes&ATTRIB_CLASSBITS) ) {
      return false; // can only joint matching classes
    } else if ( hasAttribute(ATTRIB_GROUND) ) {
      return true; // no point jointing grounds
    } else if ( m_body && other->body() ) {
      transform();
      int n = m_xformedPath.numPoints();
      for ( int end=0; end<2; end++ ) {
	if ( !m_jointed[end] ) {
	  const Vec2& p = m_xformedPath.point( end ? n-1 : 0 );
	  if ( other->distanceTo( p ) <= JOINT_TOLERANCE ) {
	    //printf("jointed end %d d=%f\n",end,other->distanceTo( p ));
	    b2Vec2 pw = p;
	    pw *= 1.0f/PIXELS_PER_METREf;
	    JointDef j( m_body, other->m_body, pw );
	    world.CreateJoint( &j );
	    m_jointed[end] = true;
	  }
	}
      }
    }
    if ( m_body ) {
      return m_jointed[0] && m_jointed[1];
    }
    return true; ///nothing to do
  }

  void draw( Canvas& canvas )
  {
    if ( m_hide < HIDE_STEPS ) {
      transform();
      canvas.drawPath( m_xformedPath, canvas.makeColour(m_colour), true );
      m_drawn = true;
    }
    m_drawnBbox = m_xformBbox;
  }

  void addPoint( const Vec2& pp ) 
  {
    Vec2 p = pp; p -= m_origin;
    if ( p == m_rawPath.point( m_rawPath.numPoints()-1 ) ) {
    } else {
      m_rawPath.append( p );
      m_drawn = false;
    }
  }

  void origin( const Vec2& p ) 
  {
    // todo 
    if ( m_body ) {
      b2Vec2 pw = p;
      pw *= 1.0f/PIXELS_PER_METREf;
      m_body->SetOriginPosition( pw, m_body->GetRotation() );
    }
    m_origin = p;
  }

  b2Body* body() { return m_body; }

  float distanceTo( const Vec2& pt )
  {
    float best = 100000.0;
    transform();
    for ( int i=1; i<m_xformedPath.numPoints(); i++ ) {    
      Segment s( m_xformedPath.point(i-1), m_xformedPath.point(i) );
      float d = s.distanceTo( pt );
      //printf("  d[%d]=%f %d,%d\n",i,d,m_rawPath.point(i-1).x,m_rawPath.point(i-1).y);
      if ( d < best ) {
        best = d;
      }
    }
    return best;
  }

  Rect bbox() 
  {
    transform();
    return m_xformBbox;
  }

  Rect lastDrawnBbox() 
  {
    return m_drawnBbox;
  }

  bool isDirty()
  {
    return !m_drawn || transform();
  }

  void hide()
  {
    if ( m_hide==0 ) {
      m_hide = 1;
      
      if (m_body) {
	// stash the body where no-one will find it
	m_body->SetCenterPosition( b2Vec2(0.0f,CANVAS_HEIGHTf*2.0f), 0.0f );
	m_body->SetLinearVelocity( b2Vec2(0.0f,0.0f) );
	m_body->SetAngularVelocity( 0.0f );
      }
    }
  }

  bool hidden()
  {
    return m_hide >= HIDE_STEPS;
  }

  int numPoints()
  {
    return m_rawPath.numPoints();
  }

private:
  static float vec2Angle( b2Vec2 v ) 
  {
    float a=atan(v.y/v.x);
    return v.y>0?a:a+b2_pi;
  } 

  void process()
  {
    float thresh = 0.1*SIMPLIFY_THRESHOLDf;
    m_rawPath.simplify( thresh );
    m_shapePath = m_rawPath;

    while ( m_shapePath.numPoints() > MULTI_VERTEX_LIMIT ) {
      thresh += SIMPLIFY_THRESHOLDf;
      m_shapePath.simplify( thresh );
    }
  }

  bool transform()
  {
    // distinguish between xformed raw and shape path as needed
    if ( m_hide ) {
      if ( m_hide < HIDE_STEPS ) {
	//printf("hide %d\n",m_hide);
	Vec2 o = m_xformBbox.centroid();
	m_xformedPath -= o;
	m_xformedPath.scale( 0.99 );
	m_xformedPath += o;
	m_xformBbox = m_xformedPath.bbox();
	m_hide++;
	return true;
      }
    } else if ( m_body ) {
      if ( hasAttribute( ATTRIB_DECOR ) ) {
	return false; // decor never moves
      } else if ( hasAttribute( ATTRIB_GROUND )	   
		  && m_xformAngle == m_body->GetRotation() ) {
	return false; // ground strokes never move.
      } else if ( m_xformAngle != m_body->GetRotation() 
	   ||  ! (m_xformPos == m_body->GetOriginPosition()) ) {
	//printf("transform stroke - rot or pos\n");
	b2Mat22 rot( m_body->GetRotation() );
	b2Vec2 orig = PIXELS_PER_METREf * m_body->GetOriginPosition();
	m_xformedPath = m_rawPath;
	m_xformedPath.rotate( rot );
	m_xformedPath.translate( Vec2(orig) );
	m_xformAngle = m_body->GetRotation();
	m_xformPos = m_body->GetOriginPosition();
	m_xformBbox = m_xformedPath.bbox();
      } else if ( ! (m_xformPos == m_body->GetOriginPosition() ) ) {
	//NOT WORKING printf("transform stroke - pos\n");
	b2Vec2 move = m_body->GetOriginPosition() - m_xformPos;
	move *= PIXELS_PER_METREf;
	m_xformedPath.translate( Vec2(move) );
	m_xformPos = m_body->GetOriginPosition();
	m_xformBbox = m_xformedPath.bbox();//TODO translate instead of recalc
      } else {
	//printf("transform none\n");
	return false;
      }
    } else {
      //printf("transform no body\n");
      m_xformedPath = m_rawPath;
      m_xformedPath.translate( m_origin );
      m_xformBbox = m_xformedPath.bbox();      
      return false;
    }
    return true;
  }

  Path      m_rawPath;
  int       m_colour;
  int       m_attributes;
  Vec2      m_origin;
  Path      m_shapePath;
  Path      m_xformedPath;
  float     m_xformAngle;
  b2Vec2    m_xformPos;
  Rect      m_xformBbox;
  Rect      m_drawnBbox;
  bool      m_drawn;
  b2Body*   m_body;
  bool      m_jointed[2];
  int       m_hide;
};


class Scene
{
public:

  Scene( bool noWorld=false )
    : m_world( NULL ),
      m_bgImage( NULL ),
      m_protect( 0 )
  {
    if ( !noWorld ) {
      b2AABB worldAABB;
      worldAABB.minVertex.Set(-100.0f, -100.0f);
      worldAABB.maxVertex.Set(100.0f, 100.0f);
      
      b2Vec2 gravity(0.0f, 10.0f);
      bool doSleep = true;
      m_world = new b2World(worldAABB, gravity, doSleep);
    }
  }

  ~Scene()
  {
    clear();
    delete m_world;
  }

  int numStrokes()
  {
    return m_strokes.size();
  }

  Stroke* newStroke( const Path& p ) {
    Stroke *s = new Stroke(p);
    m_strokes.append( s );
    return s;
  }

  void deleteStroke( Stroke *s ) {
    //printf("delete stroke %p\n",s);	  
    if ( s ) {
      int i = m_strokes.indexOf(s);
      if ( i >= m_protect ) {
	reset(s);
	m_strokes.erase( m_strokes.indexOf(s) );
      }
    }
  }
	

  void activate( Stroke *s )
  {
    //printf("activate stroke\n");
    s->createBodies( *m_world );
    createJoints( s );
  }

  void activateAll()
  {
    for ( int i=0; i < m_strokes.size(); i++ ) {
      m_strokes[i]->createBodies( *m_world );
    }
    for ( int i=0; i < m_strokes.size(); i++ ) {
      createJoints( m_strokes[i] );
    }
  }

  void createJoints( Stroke *s )
  {
    for ( int j=m_strokes.size()-1; j>=0; j-- ) {      
      if ( s != m_strokes[j] ) {
	//printf("try join to %d\n",j);
	s->maybeCreateJoint( *m_world, m_strokes[j] );
	m_strokes[j]->maybeCreateJoint( *m_world, s );
      }
    }    
  }

  void step()
  {
    m_world->Step( ITERATION_TIMESTEPf, SOLVER_ITERATIONS );
    
    // check for completion
    for (b2Contact* c = m_world->GetContactList(); c; c = c->GetNext()) {
      if (c->GetManifoldCount() > 0) {
	Stroke* s1 = (Stroke*)c->GetShape1()->GetBody()->GetUserData();
	Stroke* s2 = (Stroke*)c->GetShape2()->GetBody()->GetUserData();
	if ( s1 && s2 ) {
	  if ( s2->hasAttribute(Stroke::ATTRIB_TOKEN) ) {
	    b2Swap( s1, s2 );
	  }
	  if ( s1->hasAttribute(Stroke::ATTRIB_TOKEN) 
	       && s2->hasAttribute(Stroke::ATTRIB_GOAL) ) {
	    s2->hide();
	    //printf("SUCCESS!! level complete\n");
	  }
	}
      }
    }

    // check for token respawn
    for ( int i=0; i < m_strokes.size(); i++ ) {
      if ( m_strokes[i]->hasAttribute( Stroke::ATTRIB_TOKEN )
	   && !BOUNDS_RECT.intersects( m_strokes[i]->lastDrawnBbox() ) ) {
	printf("RESPAWN token %d\n",i);
	reset( m_strokes[i] );
	activate( m_strokes[i] );
      }
    }
  }

  bool isCompleted()
  {
    for ( int i=0; i < m_strokes.size(); i++ ) {
      if ( m_strokes[i]->hasAttribute( Stroke::ATTRIB_GOAL )
	   && !m_strokes[i]->hidden() ) {
	return false;
      }
    }
    //printf("completed!\n");
    return true;
  }

  Rect dirtyArea()
  {
    Rect r(0,0,0,0),temp;
    int numDirty = 0;
    for ( int i=0; i<m_strokes.size(); i++ ) {
      if ( m_strokes[i]->isDirty() ) {
	// acumulate new areas to draw
	temp = m_strokes[i]->bbox();
	if ( !temp.isEmpty() ) {
	  if ( numDirty==0 ) {	
	    r = temp;
	  } else {
	    r.expand( m_strokes[i]->bbox() );
	  }
	  // plus prev areas to erase
	  r.expand( m_strokes[i]->lastDrawnBbox() );
	  numDirty++;
	}
      }
    }
    if ( !r.isEmpty() ) {
      // expand to allow for thick lines
      r.tl.x--; r.tl.y--;
      r.br.x++; r.br.y++;
    }
    return r;
  }

  void draw( Canvas& canvas, const Rect& area )
  {
    if ( m_bgImage ) {
      canvas.setBackground( m_bgImage );
    } else {
      canvas.setBackground( 0 );
    }
    canvas.clear( area );
    Rect clipArea = area;
    clipArea.tl.x--;
    clipArea.tl.y--;
    clipArea.br.x++;
    clipArea.br.y++;
    for ( int i=0; i<m_strokes.size(); i++ ) {
      if ( area.intersects( m_strokes[i]->bbox() ) ) {
	m_strokes[i]->draw( canvas );
      }
    }
    //canvas.drawRect( area, 0xffff0000, false );
  }

  void reset( Stroke* s=NULL )
  {
    for ( int i=0; i<m_strokes.size(); i++ ) {
      if (s==NULL || s==m_strokes[i]) {
	m_strokes[i]->reset(m_world);
      }
    }    
  }

  Stroke* strokeAtPoint( const Vec2 pt, float max )
  {
    Stroke* best = NULL;
    for ( int i=0; i<m_strokes.size(); i++ ) {
      float d = m_strokes[i]->distanceTo( pt );
      //printf("stroke %d dist %f\n",i,d);
      if ( d < max ) {
	max = d;
	best = m_strokes[i];
      }
    }
    return best;
  }

  void clear()
  {
    reset();
    while ( m_strokes.size() ) {
      delete m_strokes[0];
      m_strokes.erase(0);
    }
    if ( m_world ) {
      while ( m_world->GetBodyList() ) {
	printf("body left over %p\n",m_world->GetBodyList());
	m_world->DestroyBody( m_world->GetBodyList() );	       
      }
    }
  }

  bool load( const string& file )
  {
    clear();
    if ( g_bgImage==NULL ) {
      g_bgImage = new Image("paper.jpg");
    }
    m_bgImage = g_bgImage;
    string line;
    ifstream i( file.c_str(), ios::in );
    while ( i.is_open() && !i.eof() ) {
      getline( i, line );
      //cout << "read: " << line << endl;;
      switch( line[0] ) {
      case 'T': m_title = line.substr(line.find(':')+1); break;
      case 'B': m_bg = line.substr(line.find(':')+1); break;
      case 'A': m_author = line.substr(line.find(':')+1); break;
      case 'S': m_strokes.append( new Stroke(line) ); break;
      }
    }
    i.close();
    protect();
    return true;
  }

  void protect( int n=-1 )
  {
    m_protect = (n==-1 ? m_strokes.size() : n );
  }

  bool save( const std::string& file )
  {
    printf("saving to %s\n",file.c_str());
    ofstream o( file.c_str(), ios::out );
    if ( o.is_open() ) {
      o << "Title: "<<m_title<<endl;
      o << "Author: "<<m_author<<endl;
      o << "Background: "<<m_bg<<endl;
      for ( int i=0; i<m_strokes.size(); i++ ) {
	o << m_strokes[i]->asString();
      }
      o.close();
      return true;
    } else {
      return false;
    }
  }

  Array<Stroke*>& strokes() 
  {
    return m_strokes;
  }

private:
  b2World        *m_world;
  Array<Stroke*>  m_strokes;
  string          m_title, m_author, m_bg;
  Image          *m_bgImage;
  static Image   *g_bgImage;
  int             m_protect;
};

Image *Scene::g_bgImage = NULL;


struct GameParams
{
  GameParams() : m_quit(false),
		 m_pause(false),
		 m_edit( false ),
		 m_refresh( true ),
		 m_colour( 2 ),
		 m_strokeFixed( false ),
		 m_strokeSleep( false ),
		 m_strokeDecor( false ),
		 m_levels(),
                 m_level(0)
  {}
  virtual ~GameParams() {}
  virtual bool save( char *file=NULL ) { return false; }
  virtual bool send() { return false; }
  virtual void gotoLevel( int l ) {}
  bool  m_quit;
  bool  m_pause;
  bool  m_edit;
  bool  m_refresh;
  int   m_colour;
  bool  m_strokeFixed;
  bool  m_strokeSleep;
  bool  m_strokeDecor;
  Levels m_levels;
  int    m_level;
};


class Overlay
{
public:
  Overlay( GameParams& game, int x=10, int y=10 )
    : m_game(game),
      m_x(x), m_y(y),
      m_canvas(NULL),
      m_dragging(false),
      m_buttonDown(false)
  {}
  virtual ~Overlay() {}

  Rect dirtyArea() 
  {
    return Rect(m_x,m_y,m_x+m_canvas->width()-1,m_y+m_canvas->height()-1);
  }
 
  virtual void onShow() {}
  virtual void onHide() {}
  virtual void onTick( int tick ) {}

  virtual void draw( Canvas& screen )
  {
    if ( m_canvas ) {
      screen.drawImage( m_canvas, m_x, m_y );
    }
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
	} else if ( ABS(ev.button.x-m_orgx) < CLICK_TOLERANCE
		    && ABS(ev.button.y-m_orgy) < CLICK_TOLERANCE ){
	  onClick( m_orgx-m_x, m_orgy-m_y );
	}
	m_buttonDown = false;
      }
      break;
    case SDL_MOUSEMOTION:
      if ( !m_dragging
	   && m_buttonDown
	   && ( ABS(ev.button.x-m_orgx) >= CLICK_TOLERANCE
		|| ABS(ev.button.y-m_orgy) >= CLICK_TOLERANCE ) ) {
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
  
  virtual bool onClick( int x, int y ) { return false; }

protected:
  GameParams& m_game;
  int     m_x, m_y;
  Canvas *m_canvas;
private:
  int     m_orgx, m_orgy;
  int     m_prevx, m_prevy;
  bool    m_dragging;
  bool    m_buttonDown;
};


class IconOverlay: public Overlay
{
public:
  IconOverlay(GameParams& game, const char* file, int x=100,int y=20)
    : Overlay(game,x,y)
  {
    m_canvas = new Image(file);
  }
};


class NextLevelOverlay : public IconOverlay
{
public:
  NextLevelOverlay( GameParams& game )
    : IconOverlay(game,"next.png",CANVAS_WIDTH/2-200,100),
      m_levelIcon(-2),
      m_icon(NULL)
  {}
  ~NextLevelOverlay()
  {
    delete m_icon;
  }
  virtual void onShow()
  {
    m_selectedLevel = m_game.m_level+1;
  }
  virtual void draw( Canvas& screen )
  {
    screen.fade();
    IconOverlay::draw( screen );
    if ( genIcon() ) {
      screen.drawImage( m_icon, m_x+100, m_y+75 );
    }
  }
  virtual bool onClick( int x, int y )
  {
    if ( y > 180 ) {
      m_game.gotoLevel( m_selectedLevel );
    } else if ( x > 300 ) {
      m_selectedLevel++;
      printf("NextLevel++ = %d\n",m_selectedLevel);
    } else if ( x < 100 && m_selectedLevel > 0 ) {
      m_selectedLevel--; 
      printf("NextLevel-- = %d\n",m_selectedLevel);
   }
    return true;
  }
private:
  bool genIcon()
  {
    if ( m_icon==NULL || m_levelIcon != m_selectedLevel ) {
      printf("new thumbnail required\n");
      delete m_icon;
      m_icon = NULL;
      if ( m_selectedLevel < m_game.m_levels.numLevels() ) {
	Scene scene( true );
	if ( scene.load( m_game.m_levels.levelFile(m_selectedLevel) ) ) {
	  printf("generating thumbnail %s\n",
		 m_game.m_levels.levelFile(m_selectedLevel).c_str());
	  Canvas temp( CANVAS_WIDTH, CANVAS_HEIGHT );
	  scene.draw( temp, FULLSCREEN_RECT );
	  m_icon = temp.scale( ICON_SCALE_FACTOR );
	  printf("generating thumbnail %s done\n",
		 m_game.m_levels.levelFile(m_selectedLevel).c_str());
	} else {
	  printf("failed to gen scene thumbnail %s\n",
		 m_game.m_levels.levelFile(m_selectedLevel).c_str());
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
  int     m_selectedLevel;
  int     m_levelIcon;
  Canvas* m_icon;
  string  m_caption;
};


class DemoOverlay : public IconOverlay
{
  struct EvEntry {
    int t;
    SDL_Event e;
  };
  typedef enum {
    STARTING,
    RUNNING,
    PLAYING,
    STOPPED
  } State;
    
public:
  DemoOverlay( GameParams& game )
    : IconOverlay( game, "pause.png" ),
      m_state( STARTING ),
      m_log( 512 )
  {
  }
  virtual void onTick( int tick )
  {
    switch ( m_state ) {
    case STARTING:
      m_game.save( DEMO_TEMP_FILE );
      //todo reset motion: m_game.load( DEMO_TEMP_FILE );
      m_t0 = tick;
      m_state = RUNNING;
      m_log.empty();
      break;
    case RUNNING:
      m_tnow = tick;
      break;
    case PLAYING:
      while ( m_playidx < m_log.size()
	      && m_log[m_playidx].t < tick-m_t0 ) {
	SDL_PushEvent( &m_log[m_playidx++].e );
      }  
    break;
    }
  }
  virtual bool onClick( int x, int y )
  {
    switch ( m_state ) {
    case STOPPED:
    case RUNNING:
      break;
    case PLAYING:
      m_state = STOPPED;
      break;
    }
    return true;
  }
  virtual void draw( Canvas& screen )
  {
    IconOverlay::draw( screen );
    switch ( m_state ) {
    case STOPPED:
    case RUNNING:
    case PLAYING:
      break;
    }
  }
  virtual bool handleEvent( SDL_Event& ev )
  {
    if ( IconOverlay::handleEvent(ev) ) {
      return true;
    } else if ( m_state == RUNNING ) {
      EvEntry e = { m_tnow - m_t0, ev };
      m_log.append( e );
    }
  }

private:
  State            m_state;
  int  		   m_t0, m_tnow;
  int              m_playidx;
  Array<EvEntry>   m_log;
};


class EditOverlay : public IconOverlay
{
  int m_saving, m_sending;
public:
  EditOverlay( GameParams& game )
    : IconOverlay(game,"edit.png"),
      m_saving(0), m_sending(0)
      
  {
    for ( int i=0; i<NUM_COLOURS; i++ ) {
      m_canvas->drawRect( pos(i), m_canvas->makeColour(brush_colours[i]), true );
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
    if ( m_sending-- ) outline( screen, 16, screen.makeColour(m_sending<<9) );
    if ( m_saving-- )  outline( screen, 17, screen.makeColour(m_saving<<9) );
  }
  virtual bool onClick( int x, int y )
  {
    int i = index(x,y);
    switch (i) {
    case -1: return false;
    case 12: m_game.m_strokeFixed = ! m_game.m_strokeFixed; break;
    case 13: m_game.m_strokeSleep = ! m_game.m_strokeSleep; break;
    case 14: m_game.m_strokeDecor = ! m_game.m_strokeDecor; break;
    case 16: if ( m_game.send() ) m_sending=10; break;
    case 17: if ( m_game.save() ) m_saving=10; break;
    default: if (i<NUM_COLOURS) m_game.m_colour = i; break;
    }
    return true;
  }
};


class Game : public GameParams
{
  Scene   	    m_scene;
  Stroke  	   *m_createStroke;
  Stroke           *m_moveStroke;
  Array<Overlay*>   m_overlays;
  Window            m_window;
  IconOverlay       m_pauseOverlay;
  EditOverlay       m_editOverlay;
#ifdef USE_HILDON
  Hildon            m_hildon;
#endif //USE_HILDON
public:
  Game( int argc, const char** argv ) 
  : m_createStroke(NULL),
    m_moveStroke(NULL),
    m_window(800,480,"Numpty Physics","NPhysics"),
    m_pauseOverlay( *this, "pause.png",50,50),
    m_editOverlay( *this)
  {
    if ( argc<=1 ) {
      FILE *f = fopen("Game.cpp","rt");
      if ( f ){
        m_levels.addPath( "." );
	fclose(f);
      } else {
        m_levels.addPath( DEFAULT_LEVEL_PATH );
      }
      std::string u( getenv("HOME") );
      m_levels.addPath( (u + "/" USER_LEVEL_PATH).c_str() );
    } else {
      for ( int i=1;i<argc;i++ ) {
	m_levels.addPath( argv[i] );
      }
    }
    gotoLevel( 0 );
  }

  void gotoLevel( int l )
  {
    if ( l >= 0 && l < m_levels.numLevels() ) {
      printf("loading from %s\n",m_levels.levelFile(l).c_str());
      m_scene.load( m_levels.levelFile(l).c_str() );
      m_scene.activateAll();
      m_level = l;
      m_refresh = true;
      if ( m_edit ) {
	m_scene.protect(0);
      }
    }
  }

  bool save( char *file=NULL )
  {	  
    string p;
    if ( file ) {
      p = file;
    } else {
      p = getenv("HOME"); p+="/"USER_BASE_PATH"/L99_saved.nph";
    }
    if ( m_scene.save( p ) ) {
      m_levels.addPath( p.c_str() );
      return true;
    }
    return false;
  }

  bool send()
  {
    return save();
  }

  void setTool( int t )
  {
    m_colour = t;
  }

  void editMode( bool set )
  {
    m_edit = set;
  }

  void showOverlay( Overlay& o )
  {
    printf("show overlay\n");
    m_overlays.append( &o );
    o.onShow();
  }

  void hideOverlay( Overlay& o )
  {
    printf("hide overlay\n");
    o.onHide();
    m_overlays.erase( m_overlays.indexOf(&o) );
    m_refresh = true;
  }

  void pause( bool doPause )
  {
    if ( m_pause != doPause ) {
      m_pause = doPause;
      if ( m_pause ) {
	showOverlay( m_pauseOverlay );
      } else {
	hideOverlay( m_pauseOverlay );
      }
    }
  }

  void edit( bool doEdit )
  {
    if ( m_edit != doEdit ) {
      m_edit = doEdit;
      if ( m_edit ) {
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
    return Vec2( ev.button.x, ev.button.y );
  }

  bool handleGameEvent( SDL_Event &ev )
  {
    switch( ev.type ) {
    case SDL_QUIT:
      m_quit = true;
      break;
    case SDL_KEYDOWN:
      switch ( ev.key.keysym.sym ) {
      case SDLK_q:
	m_quit = true;
	break;
      case SDLK_SPACE:
      case SDLK_KP_ENTER:
      case SDLK_RETURN:
	pause( !m_pause );
	break;
      case SDLK_s:
      case SDLK_F4: 
	save();
	break;
      case SDLK_e:
      case SDLK_F6:
	edit( !m_edit );
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
      case SDLK_x:
	//record();
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
    static bool mod=0;
    //printf("mod=%d\n",ev.key.keysym.sym,mod);
    switch( ev.type ) {      
    case SDL_KEYDOWN:
      //printf("mod key=%x mod=%d\n",ev.key.keysym.sym,mod);
      if ( ev.key.keysym.sym == SDLK_F8 ) {
	mod = 1;  //zoom- == middle (delete)
	printf("button mod =1\n");
	return true;
      } else if ( ev.key.keysym.sym == SDLK_F7 ) {
	mod = 2;  //zoom+ == right (move)
	printf("button mod =2\n");
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
	printf("button mod=%d click but=%d\n",mod,ev.button.button);
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
	m_createStroke = m_scene.newStroke( Path()&mousePoint(ev) );
	if ( m_createStroke ) {
	  switch ( m_colour ) {
	  case 0: m_createStroke->setAttribute( Stroke::ATTRIB_TOKEN ); break;
	  case 1: m_createStroke->setAttribute( Stroke::ATTRIB_GOAL ); break;
	  default: m_createStroke->setColour( brush_colours[m_colour] ); break;
	  }
	  if ( m_strokeFixed ) {
	    m_createStroke->setAttribute( Stroke::ATTRIB_GROUND );
	  }
	  if ( m_strokeSleep ) {
	    m_createStroke->setAttribute( Stroke::ATTRIB_SLEEPING );
	  }
	  if ( m_strokeDecor ) {
	    m_createStroke->setAttribute( Stroke::ATTRIB_DECOR );
	  }
	}
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if ( ev.button.button == SDL_BUTTON_LEFT
	   && m_createStroke ) {
	if ( m_createStroke->numPoints() > 1 ) {
	  m_scene.activate( m_createStroke );
	} else {
	  m_scene.deleteStroke( m_createStroke );
	}
	m_createStroke = NULL;
      }
      break;
    case SDL_MOUSEMOTION:
      if ( m_createStroke ) {
	m_createStroke->addPoint( mousePoint(ev) );
      }
      break;
    case SDL_KEYDOWN:
      if ( ev.key.keysym.sym == SDLK_ESCAPE ) {
	if ( m_createStroke ) {
	  m_scene.deleteStroke( m_createStroke );
	  m_createStroke = NULL;
	} else {
	  m_scene.deleteStroke( m_scene.strokes().at(m_scene.strokes().size()-1) );
	}
	m_refresh = true;
      }
      break;
    default:
      break;
    }
    return false;
  }

  bool handleEditEvent( SDL_Event &ev )
  {
    if ( !m_edit ) return false;

    switch( ev.type ) {      
    case SDL_MOUSEBUTTONDOWN: 
      if ( ev.button.button == SDL_BUTTON_RIGHT
	   && !m_moveStroke ) {
	m_moveStroke = m_scene.strokeAtPoint( mousePoint(ev),
					      SELECT_TOLERANCE );
      } else if ( ev.button.button == SDL_BUTTON_MIDDLE ) {
	m_scene.deleteStroke( m_scene.strokeAtPoint( mousePoint(ev),
						     SELECT_TOLERANCE ) );
	m_refresh = true;
      }
      break;
    case SDL_MOUSEBUTTONUP:
      if ( ev.button.button == SDL_BUTTON_RIGHT
	   && m_moveStroke ) {
	m_moveStroke = NULL;
      }
      break;
    case SDL_MOUSEMOTION:
      if ( m_moveStroke ) {
	m_moveStroke->origin( mousePoint(ev) );
      }
      break;
    default:
      break;
    }
    return false;
  }

  void run()
  {
    NextLevelOverlay  completedOverlay(*this);

    m_scene.draw( m_window, FULLSCREEN_RECT );
    m_window.update( FULLSCREEN_RECT );

    int iterateCounter = 0;
    int lastTick = SDL_GetTicks();
    bool isComplete = false;

    while ( !m_quit ) {
      for ( int i=0; i<m_overlays.size(); i++ ) {
	m_overlays[i]->onTick( lastTick );
      }

      SDL_Event ev;
      while ( SDL_PollEvent(&ev) ) { 
	bool handled = false;
	for ( int i=m_overlays.size()-1; i>=0 && !handled; i-- ) {
	  handled = m_overlays[i]->handleEvent(ev);
	}
	if ( !handled ) {
	  handled = false
	    || handleModEvent(ev)
	    || handleGameEvent(ev)
	    || handleEditEvent(ev)
	    || handlePlayEvent(ev);
	}
      }

      if ( isComplete && m_edit ) {
	hideOverlay( completedOverlay );
	isComplete = false;
      }
      if ( m_scene.isCompleted() != isComplete && !m_edit ) {
	isComplete = m_scene.isCompleted();
	if ( isComplete ) {
	  showOverlay( completedOverlay );
	} else {
	  hideOverlay( completedOverlay );
	}
      }

      Rect r = m_scene.dirtyArea();
      if ( m_refresh || isComplete ) {
	r = FULLSCREEN_RECT;
      }

      if ( !r.isEmpty() ) {
	m_scene.draw( m_window, r );
      }
      
      for ( int i=0; i<m_overlays.size(); i++ ) {
	m_overlays[i]->draw( m_window );
	r.expand( m_overlays[i]->dirtyArea() );
      }

      //temp
      if ( m_refresh ) {
	m_window.update( FULLSCREEN_RECT );
	m_refresh = false;
      } else {
	r.br.x++; r.br.y++;
	m_window.update( r );
      } 

	  
      if ( !m_pause ) {
	//assumes RENDER_RATE <= ITERATION_RATE
	while ( iterateCounter < ITERATION_RATE ) {
	  m_scene.step();
	  iterateCounter += RENDER_RATE;
	}
	iterateCounter -= ITERATION_RATE;
      }
      
#ifdef USE_HILDON
      m_hildon.poll();      
      for ( char *f = m_hildon.getFile(); f; f=m_hildon.getFile() ) {
	if ( f ) {
	  m_levels.addPath( f );
	  int l = m_levels.findLevel( f );
	  if ( l >= 0 ) {
	    gotoLevel( l );
	  }
	}
      }      
#endif //USE_HILDON
      int sleepMs = lastTick + RENDER_INTERVAL -  SDL_GetTicks();
      if ( sleepMs > 0 ) {
	SDL_Delay( sleepMs );
	//printf("sleep %dms\n",sleepMs);
      } else {
	printf("overrun %dms\n",-sleepMs);
      }
      lastTick = SDL_GetTicks();      
    }
  }
};


int main(int argc, char** argv)
{
  try {
    putenv("SDL_VIDEO_X11_WMCLASS=NPhysics");
    if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
      throw "Couldn't initialize SDL";
    }

    if ( mkdir( (string(getenv("HOME"))+"/"USER_BASE_PATH).c_str(),
		0755)==0 ) {
      printf("created user dir\n");
    } else if ( errno==EEXIST ){
      printf("user dir ok\n");
    } else {
      printf("failed to create user dir\n");
    }


    if ( argc > 2 && strcmp(argv[1],"-bmp")==0 ) {
      for ( int i=2; i<argc; i++ ) {
	Scene scene( true );
	if ( scene.load( argv[i] ) ) {
	  printf("generating bmp %s\n", argv[i]);
	  Canvas temp( CANVAS_WIDTH, CANVAS_HEIGHT );
	  scene.draw( temp, FULLSCREEN_RECT );
	  string bmp( argv[i] );
	  bmp += ".bmp";
	  temp.writeBMP( bmp.c_str() );
	}	
      }
    } else {
      Game game( argc, (const char**)argv );
      game.run();
    }
  } catch ( const char* e ) {
    cout <<"*** CAUGHT: "<<e<<endl;
  } 
  return 0;
}


