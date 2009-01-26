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
#include "Levels.h"
#include "Http.h"
#include "Os.h"

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

class Transform
{
public:
  Transform( float32 scale, float32 rotation, const Vec2& translation )
  {
    set( scale, rotation, translation );
  }
  void set( float32 scale, float32 rotation, const Vec2& translation )
  {
    if ( scale==0.0f && rotation==0.0f && translation==Vec2(0,0) ) {
      m_bypass = true;
    } else {
      m_rot.Set( rotation );
      m_pos = translation;
      m_rot.col1.x *= scale;
      m_rot.col1.y *= scale;
      m_rot.col2.x *= scale;
      m_rot.col2.y *= scale;
      m_invrot = m_rot.Invert();
      m_bypass = false;
    }
  }
  inline void transform( const Path& pin, Path& pout ) {
    pout = pin;
    if ( !m_bypass ) {
      pout.rotate( m_rot );
      pout.translate( m_pos );
    }
  }
  inline void transform( Vec2& vec ) {
    if ( !m_bypass ) {
      vec = Vec2( b2Mul( m_rot, vec ) ) + m_pos;
    }
  }
  inline void inverseTransform( Vec2& vec ) {
    if ( !m_bypass ) {
      vec = Vec2( b2Mul( m_invrot, vec-m_pos ) );
    }
  }
private:
  Transform() {}
  bool m_bypass;
  b2Mat22 m_rot;
  b2Mat22 m_invrot;
  Vec2 m_pos;
};

Transform worldToScreen( 0.5f, M_PI/2, Vec2(240,0) );

void configureScreenTransform( int w, int h )
{
  SCREEN_WIDTH = w;
  SCREEN_HEIGHT = h;
  FULLSCREEN_RECT = Rect(0,0,w-1,h-1);
  if ( w==WORLD_WIDTH && h==WORLD_HEIGHT ) { //unity
    worldToScreen.set( 0.0f, 0.0f, Vec2(0,0) );
  } else {
    float rot = 0.0f;
    Vec2 tr(0,0);
    if ( h > w ) { //portrait
      rot = M_PI/2;
      tr = Vec2( w, 0 );
      b2Swap( h, w );
    }
    float scalew = (float)w/(float)WORLD_WIDTH;
    float scaleh = (float)h/(float)WORLD_HEIGHT;
    if ( scalew < scaleh ) {
      worldToScreen.set( scalew, rot, tr );
    } else {
      worldToScreen.set( scaleh, rot, tr );
    }
  }
}

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
    ATTRIB_HIDDEN = 32,
    ATTRIB_DELETED = 64,
    ATTRIB_CLASSBITS = ATTRIB_TOKEN | ATTRIB_GOAL
  } Attribute;

private:
  struct JointDef : public b2RevoluteJointDef
  {
    JointDef( b2Body* b1, b2Body* b2, const b2Vec2& pt )
    {
      Initialize( b1, b2, pt );
      maxMotorTorque = 10.0f;
      motorSpeed = 0.0f;
      enableMotor = true;
    }
  };

  struct BoxDef : public b2PolygonDef
  {
    void init( const Vec2& p1, const Vec2& p2, int attr )
    {
      b2Vec2 barOrigin = p1;
      b2Vec2 bar = p2 - p1;
      bar *= 1.0f/PIXELS_PER_METREf;
      barOrigin *= 1.0f/PIXELS_PER_METREf;;
      SetAsBox( bar.Length()/2.0f, 0.1f,
		0.5f*bar + barOrigin, vec2Angle( bar ));
      //      SetAsBox( bar.Length()/2.0f+b2_toiSlop, b2_toiSlop*2.0f,
      //	0.5f*bar + barOrigin, vec2Angle( bar ));
      friction = 0.3f;
      if ( attr & ATTRIB_GROUND ) {
	density = 0.0f;
      } else if ( attr & ATTRIB_GOAL ) {
	density = 100.0f;
      } else if ( attr & ATTRIB_TOKEN ) {
	density = 3.0f;
	friction = 0.1f;
      } else {
	density = 5.0f;
      }
      restitution = 0.2f;
    }
  };

public:
  Stroke( const Path& path )
    : m_rawPath(path)
  {
    m_colour = brushColours[DEFAULT_BRUSH];
    m_attributes = 0;
    m_origin = m_rawPath.point(0);
    m_rawPath.translate( -m_origin );
    reset();
  }  

  Stroke( const string& str ) 
  {
    int col = 0;
    m_colour = brushColours[DEFAULT_BRUSH];
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
    if ( col >= 0 && col < NUM_BRUSHES ) {
      m_colour = brushColours[col];
    }
    if ( *s++ == ':' ) {
      m_rawPath = Path(s);
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
    for ( int i=0; i<NUM_BRUSHES; i++ ) {
      if ( m_colour==brushColours[i] )  s<<i;
    }
    s << ":";
    transform();
    for ( int i=0; i<m_xformedPath.size(); i++ ) {
      const Vec2& p = m_xformedPath.point(i);
      s <<' '<< p.x << ',' << p.y; 
    }
    s << endl;
    return s.str();
  }

  void setAttribute( Stroke::Attribute a )
  {
    m_attributes |= a;
    if ( m_attributes & ATTRIB_TOKEN )     m_colour = brushColours[RED_BRUSH];
    else if ( m_attributes & ATTRIB_GOAL ) m_colour = brushColours[YELLOW_BRUSH];
  }

  void clearAttribute( Stroke::Attribute a )
  {
    m_attributes &= ~a;
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
      b2BodyDef bodyDef;
      bodyDef.position = m_origin;
      bodyDef.position *= 1.0f/PIXELS_PER_METREf;
      bodyDef.userData = this;
      if ( m_attributes & ATTRIB_SLEEPING ) {
	bodyDef.isSleeping = true;
      }
      m_body = world.CreateBody( &bodyDef );
      for ( int i=1; i<n; i++ ) {
	BoxDef boxDef;
	boxDef.init( m_shapePath.point(i-1),
		     m_shapePath.point(i),
		     m_attributes );
	m_body->CreateShape( &boxDef );
      }
      m_body->SetMassFromShapes();

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
      bool thick = (canvas.width() > 400);
      transform();
      canvas.drawPath( m_screenPath, canvas.makeColour(m_colour), thick );
      m_drawn = true;
    }
    m_drawnBbox = m_screenBbox;
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
      m_body->SetXForm( pw, m_body->GetAngle() );
    }
    m_origin = p;
  }

  b2Body* body() { return m_body; }

  float32 distanceTo( const Vec2& pt )
  {
    float32 best = 100000.0;
    transform();
    for ( int i=1; i<m_xformedPath.numPoints(); i++ ) {    
      Segment s( m_xformedPath.point(i-1), m_xformedPath.point(i) );
      float32 d = s.distanceTo( pt );
      //printf("  d[%d]=%f %d,%d\n",i,d,m_rawPath.point(i-1).x,m_rawPath.point(i-1).y);
      if ( d < best ) {
        best = d;
      }
    }
    return best;
  }

  Rect screenBbox() 
  {
    transform();
    return m_screenBbox;
  }

  Rect lastDrawnBbox() 
  {
    return m_drawnBbox;
  }

  Rect worldBbox() 
  {
    return m_xformedPath.bbox();
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
	m_body->SetXForm( b2Vec2(0.0f,SCREEN_HEIGHT*2.0f), 0.0f );
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
  static float32 vec2Angle( b2Vec2 v ) 
  {
    return b2Atan2(v.y, v.x);
  } 

  void process()
  {
    float32 thresh = 0.1*SIMPLIFY_THRESHOLDf;
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
	Vec2 o = m_screenBbox.centroid();
	m_screenPath -= o;
	m_screenPath.scale( 0.99 );
	m_screenPath += o;
	m_screenBbox = m_screenPath.bbox();      
	m_hide++;
	return true;
      }
    } else if ( m_body ) {
      if ( hasAttribute( ATTRIB_DECOR ) ) {
	return false; // decor never moves
      } else if ( hasAttribute( ATTRIB_GROUND )	   
		  && m_xformAngle == m_body->GetAngle() ) {
	return false; // ground strokes never move.
      } else if ( m_xformAngle != m_body->GetAngle() 
	   ||  ! (m_xformPos == m_body->GetPosition()) ) {
	//printf("transform stroke - rot or pos\n");
	b2Mat22 rot( m_body->GetAngle() );
	b2Vec2 orig = PIXELS_PER_METREf * m_body->GetPosition();
	m_xformedPath = m_rawPath;
	m_xformedPath.rotate( rot );
	m_xformedPath.translate( Vec2(orig) );
	m_xformAngle = m_body->GetAngle();
	m_xformPos = m_body->GetPosition();
	worldToScreen.transform( m_xformedPath, m_screenPath );
	m_screenBbox = m_screenPath.bbox();      
      } else {
	//printf("transform none\n");
	return false;
      }
    } else {
      //printf("transform no body\n");
      m_xformedPath = m_rawPath;
      m_xformedPath.translate( m_origin );
      worldToScreen.transform( m_xformedPath, m_screenPath );
      m_screenBbox = m_screenPath.bbox();      
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
  Path      m_screenPath;
  float32   m_xformAngle;
  b2Vec2    m_xformPos;
  Rect      m_screenBbox;
  Rect      m_drawnBbox;
  bool      m_drawn;
  b2Body*   m_body;
  bool      m_jointed[2];
  int       m_hide;
};


class Scene : private b2ContactListener
{
public:

  Scene( bool noWorld=false )
    : m_world( NULL ),
      m_bgImage( NULL ),
      m_protect( 0 )
  {
    if ( !noWorld ) {
      b2AABB worldAABB;
      worldAABB.lowerBound.Set(-100.0f, -100.0f);
      worldAABB.upperBound.Set(100.0f, 100.0f);
      
      b2Vec2 gravity(0.0f, 10.0f);
      bool doSleep = true;
      m_world = new b2World(worldAABB, gravity, doSleep);
      m_world->SetContactListener( this );
    }
  }

  ~Scene()
  {
    clear();
    if ( m_world ) {
      step();
      delete m_world;
    }
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
    // clean up delete strokes
    for ( int i=0; i< m_strokes.size(); i++ ) {
      if ( m_strokes[i]->hasAttribute(Stroke::ATTRIB_DELETED) ) {
	m_strokes[i]->clearAttribute(Stroke::ATTRIB_DELETED);
	m_strokes[i]->hide();
      }	   
    }
    // check for token respawn
    for ( int i=0; i < m_strokes.size(); i++ ) {
      if ( m_strokes[i]->hasAttribute( Stroke::ATTRIB_TOKEN )
	   && !BOUNDS_RECT.intersects( m_strokes[i]->worldBbox() ) ) {
	reset( m_strokes[i] );
	activate( m_strokes[i] );	  
      }
    }
  }

  // b2ContactListener callback when a new contact is detected
  virtual void Add(const b2ContactPoint* point) 
  {     
    // check for completion
    //if (c->GetManifoldCount() > 0) {
    Stroke* s1 = (Stroke*)point->shape1->GetBody()->GetUserData();
    Stroke* s2 = (Stroke*)point->shape2->GetBody()->GetUserData();
    if ( s1 && s2 ) {
      if ( s2->hasAttribute(Stroke::ATTRIB_TOKEN) ) {
	b2Swap( s1, s2 );
      }
      if ( s1->hasAttribute(Stroke::ATTRIB_TOKEN) 
	   && s2->hasAttribute(Stroke::ATTRIB_GOAL) ) {
	s2->setAttribute(Stroke::ATTRIB_DELETED);
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
	temp = m_strokes[i]->screenBbox();
	if ( !temp.isEmpty() ) {
	  if ( numDirty==0 ) {	
	    r = temp;
	  } else {
	    r.expand( m_strokes[i]->screenBbox() );
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
      if ( area.intersects( m_strokes[i]->screenBbox() ) ) {
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

  Stroke* strokeAtPoint( const Vec2 pt, float32 max )
  {
    Stroke* best = NULL;
    for ( int i=0; i<m_strokes.size(); i++ ) {
      float32 d = m_strokes[i]->distanceTo( pt );
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
      //step is required to actually destroy bodies and joints
      m_world->Step( ITERATION_TIMESTEPf, SOLVER_ITERATIONS );
    }
  }



  bool load( unsigned char *buf, int bufsize )
  {
    string s( (const char*)buf, bufsize );
    stringstream in( s, ios::in );
    return load( in );
  }

  bool load( const string& file )
  {
    ifstream in( file.c_str(), ios::in );
    return load( in ); 
  }

  bool load( istream& in )
  {
    clear();
    if ( g_bgImage==NULL ) {
      g_bgImage = new Image("paper.jpg");
      g_bgImage->scale( SCREEN_WIDTH, SCREEN_HEIGHT );
    }
    m_bgImage = g_bgImage;
    string line;
    while ( !in.eof() ) {
      getline( in, line );
      parseLine( line );
    }
    protect();
    return true;
  }

  bool parseLine( const string& line )
  {
    switch( line[0] ) {
    case 'T': m_title = line.substr(line.find(':')+1);  return true;
    case 'B': m_bg = line.substr(line.find(':')+1);     return true;
    case 'A': m_author = line.substr(line.find(':')+1); return true;
    case 'S': m_strokes.append( new Stroke(line) );     return true;
    }
    return false;
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
    m_lastTickTime = SDL_GetTicks();
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
      m_lastTickTime = SDL_GetTicks();
    }
  }

  bool fetchEvent( SDL_Event& ev )
  {
    if ( m_playing
	 && m_index < m_log->size()
	 && m_log->at(m_index).t <= m_lastTick ) {
      ev = m_log->at(m_index).e;
      m_index++;
      return true;
    }
    return false;
  }
  
private:
  bool           m_playing;
  const DemoLog* m_log;
  int            m_index;
  int  		 m_lastTick;
  int  		 m_lastTickTime;
};


class Game : public GameControl
{
  Scene   	    m_scene;
  Stroke  	   *m_createStroke;
  Stroke           *m_moveStroke;
  Array<Overlay*>   m_overlays;
  Window            m_window;
  Overlay          *m_pauseOverlay;
  Overlay          *m_editOverlay;
  //  DemoOverlay       m_demoOverlay;
  DemoRecorder      m_recorder;
  DemoPlayer        m_player;
  Os               *m_os;
public:
  Game( int width, int height ) 
  : m_createStroke(NULL),
    m_moveStroke(NULL),
    m_window(width,height,"Numpty Physics","NPhysics"),
    m_pauseOverlay( NULL ),
    m_editOverlay( NULL ),
    m_os( Os::get() )
    //,m_demoOverlay( *this )
  {
    configureScreenTransform( m_window.width(), m_window.height() );
  }


  virtual bool renderScene( Canvas& c, int level ) 
  {
    Scene scene( true );
    int size = m_levels.load( level, levelbuf, sizeof(levelbuf) );
    if ( size && scene.load( levelbuf, size ) ) {
      scene.draw( c, FULLSCREEN_RECT );
      return true;
    }
    return false;
  }

  void gotoLevel( int level, bool replay=false )
  {
    if ( level >= 0 && level < m_levels.numLevels() ) {
      int size = m_levels.load( level, levelbuf, sizeof(levelbuf) );
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
      m_levels.addPath( p.c_str() );
      int l = m_levels.findLevel( p.c_str() );
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
      if ( h.post( Config::planetRoot().c_str(), "upload", SEND_TEMP_FILE ) ) {
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
    m_overlays.append( o );
    o->onShow();
  }

  void hideOverlay( Overlay* o )
  {
    o->onHide();
    m_overlays.erase( m_overlays.indexOf(o) );
    m_refresh = true;
  }
  
  void toggleOverlay( Overlay* o )
  {
    if ( m_overlays.indexOf( o ) >= 0 ) {
      hideOverlay( o );
    } else {
      showOverlay( o );
    }
  }

  bool isPaused()
  {
    return m_pauseOverlay != NULL
      && m_overlays.indexOf( m_pauseOverlay ) >= 0;
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
	if ( !m_pauseOverlay ) {
	  m_pauseOverlay = createIconOverlay( *this, "pause.png", 50, 50 );
	}
	toggleOverlay( m_pauseOverlay );
	break;
      case SDLK_s:
	save();
	break;
      case SDLK_F4: 
	showOverlay( createMenuOverlay( *this ) );
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
	m_createStroke = m_scene.newStroke( Path()&mousePoint(ev) );
	if ( m_createStroke ) {
	  switch ( m_colour ) {
	  case 0: m_createStroke->setAttribute( Stroke::ATTRIB_TOKEN ); break;
	  case 1: m_createStroke->setAttribute( Stroke::ATTRIB_GOAL ); break;
	  default: m_createStroke->setColour( brushColours[m_colour] ); break;
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
    //allow move/delete in normal play!!
    //if ( !m_edit ) return false;

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
    Overlay *completedOverlay = createNextLevelOverlay(*this);

    m_scene.draw( m_window, FULLSCREEN_RECT );
    m_window.update( FULLSCREEN_RECT );

    int iterateCounter = 0;
    int lastTick = SDL_GetTicks();
    bool isComplete = false;

    while ( !m_quit ) {
      for ( int i=0; i<m_overlays.size(); i++ ) {
	m_overlays[i]->onTick( lastTick );
      }

      //assumes RENDER_RATE <= ITERATION_RATE
      while ( iterateCounter < ITERATION_RATE ) {
	if ( !isPaused() ) {
	  m_scene.step();
	  m_recorder.tick();
	  m_player.tick();
	}

	SDL_Event ev;
	bool more = true;
	while ( more ) {
	  more = SDL_PollEvent(&ev);
	  if ( m_player.isRunning() 
	       && ( ev.type==SDL_MOUSEMOTION || 
		    ev.type==SDL_MOUSEBUTTONDOWN || 
		    ev.type==SDL_MOUSEBUTTONUP ) ) {
	    more = false; //discard
	  }
	  if (!more) {
	    more = m_player.fetchEvent(ev);
	  }
	  if ( more ) {
	    bool handled = false;
	    m_recorder.record( ev );
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
	}
	iterateCounter += RENDER_RATE;
      }
      iterateCounter -= ITERATION_RATE;
      

      if ( isComplete && m_edit ) {
	hideOverlay( completedOverlay );
	isComplete = false;
      }
      if ( m_scene.isCompleted() != isComplete && !m_edit ) {
	isComplete = m_scene.isCompleted();
	if ( isComplete ) {
	  m_player.stop();
	  m_recorder.stop();
	  showOverlay( completedOverlay );
	} else {
	  hideOverlay( completedOverlay );
	}
      }

      Rect r = m_scene.dirtyArea();
      if ( m_refresh  ) {
	r = FULLSCREEN_RECT;
      } else {
	for ( int i=0; i<m_overlays.size(); i++ ) {
	  if ( m_overlays[i]->isDirty() ) {
	    r.expand( m_overlays[i]->dirtyArea() );
	  }
	}
      }

      if ( !r.isEmpty() ) {
	m_scene.draw( m_window, r );
	if ( m_fade ) {
	  m_window.fade( r );
	}
      }
      for ( int i=0; i<m_overlays.size(); i++ ) {
	if ( m_overlays[i]->dirtyArea().intersects(r) ) {
	  m_overlays[i]->draw( m_window );	  
	}
      }

      if ( m_refresh ) {
	m_window.update( FULLSCREEN_RECT );
	m_refresh = false;
      } else {
	r.br.x++; r.br.y++;
	//uncomment this to show dirty areas
	//m_window.drawRect( r, 0x00ff00, false );
	m_window.update( r );
      } 

	  
      if ( m_os ) {
	m_os->poll();      
	for ( char *f = m_os->getLaunchFile(); f; f=m_os->getLaunchFile() ) {
	  if ( strstr(f,".npz") ) {
	    //m_levels.empty();
	  }
	  m_levels.addPath( f );
	  int l = m_levels.findLevel( f );
	  if ( l >= 0 ) {
	    gotoLevel( l );
	    m_window.raise();
	  }
	}    
      }  

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


void init( bool useDummyVideo=false )
{
  if ( useDummyVideo ) {
    putenv((char*)"SDL_VIDEODRIVER=dummy");
  } else {
    putenv((char*)"SDL_VIDEO_X11_WMCLASS=NPhysics");
  }

  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0) {
    throw "Couldn't initialize SDL";
  }
  
  if ( mkdir( (string(getenv("HOME"))+"/"USER_BASE_PATH).c_str(),
	      0755)==0 ) {
    printf("created user dir\n");
  } else if ( errno==EEXIST ){
    //printf("user dir ok\n");
  } else {
    printf("failed to create user dir\n");
  }

}  

int npmain(int argc, char** argv)
{
  try {
    bool rotate = false;
    bool thumbnailMode = false;
    int width=SCREEN_WIDTH, height=SCREEN_HEIGHT;
    Array<const char*> files;

    for ( int i=1; i<argc; i++ ) {
      if ( strcmp(argv[i],"-bmp")==0 ) {
	thumbnailMode = true;
      } else if ( strcmp(argv[i],"-rotate")==0 ) {
       rotate = true;
      } else if ( strcmp(argv[i],"-geometry")==0 && i<argc-1) {
	int ww, hh;
	if ( sscanf(argv[i+1],"%dx%d",&ww,&hh) ==2 ) {
	  width = ww; 
	  height = hh;
	}
	i++;
      } else {
	files.append( argv[i] );
      }
    }

    if ( thumbnailMode ) {
      init(true);
      configureScreenTransform( width, height );
      for ( int i=0; i<files.size(); i++ ) {
	Scene scene( true );
	if ( scene.load( files[i] ) ) {
	  printf("generating bmp %s\n", files[i]);
	  Canvas temp( width, height );
	  scene.draw( temp, FULLSCREEN_RECT );
	  string bmp( files[i] );
	  bmp += ".bmp";
	  temp.writeBMP( bmp.c_str() );
	}
      }
    } else {      
      init();
      Game game( width, height );

      if ( files.size() > 0 ) {
	for ( int i=0; i<files.size(); i++ ) {
	  game.levels().addPath( files[i] );
	}
      } else {
	struct stat st;
	if ( stat("Game.cpp",&st)==0 ) {
	  game.levels().addPath( "data" );
	} else {
	  game.levels().addPath( DEFAULT_LEVEL_PATH );
	}
	game.levels().addPath( Config::userDataDir().c_str() );
      }
      game.gotoLevel(0);
      game.run();
    }
  } catch ( const char* e ) {
    cout <<"*** CAUGHT: "<<e<<endl;
  } 
  return 0;
}

