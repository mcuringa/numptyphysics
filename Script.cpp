/*
 * This file is part of NumptyPhysics
 * Copyright (C) 2009 Tim Edmonds
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
#include "Script.h"
#include "Path.h"
#include "Scene.h"
#include <sstream>
#include <cstdio>


ScriptEntry::ScriptEntry( const std::string& str )
{
  char opc;
  printf("log: %s\n",str.c_str());
  if ( sscanf(str.c_str(), "%d,%c,%d,%d,%d,%d,%d",
	      &t, &opc, &stroke, &arg1, &arg2, &pt.x, &pt.y)==7 ) {
    switch (opc) {
    case 'n': op = OP_NEW; break;
    case 'd': op = OP_DELETE; break;
    case 'e': op = OP_EXTEND; break;
    case 'm': op = OP_MOVE; break;
    case 'a': op = OP_ACTIVATE; break;
    default:
      fprintf(stderr,"bad script op\n");
    }
  } else {
    fprintf(stderr,"badly formed script entry\n");
  }
}


std::string ScriptEntry::asString()
{
  static const char opcodes[] = "ndema";
  std::stringstream s;
  s << t << "," << opcodes[op] << ","
    << stroke << "," << arg1 << "," << arg2 << ","
    << pt.x << "," << pt.y; 
  return s.str();
}

std::string ScriptLog::asString( int i )
{
  if ( i < size() ) {
    return at(i).asString();
  }
  return std::string();
}

void ScriptLog::append( int tick, ScriptEntry::Op op, int stroke, 
		      int arg1, int arg2, const Vec2& pt )
{
  append( ScriptEntry( tick, op, stroke, arg1, arg2, pt ) );
}

void ScriptLog::append( const std::string& str ) 
{
  append( ScriptEntry(str) );
}



ScriptRecorder::ScriptRecorder()
  : m_log(NULL),
    m_running(false)
{
}

void ScriptRecorder::start( ScriptLog* log ) 
{
  m_running = true;
  m_log = log;
  m_log->empty();
  m_log->capacity(128);
  m_lastTick = 0;
}

void ScriptRecorder::stop()  
{ 
  if ( m_running ) {
    for ( int i=0; i<m_log->size(); i++ ) {
      std::string e = m_log->asString(i);
    }
    m_running = false; 
  }
}

void ScriptRecorder::tick() 
{
  if ( m_running ) {
    m_lastTick++;
  }
}


void ScriptRecorder::newStroke( const Path& p, int colour, int attribs )
{
  if ( m_running )
    m_log->append( m_lastTick, ScriptEntry::OP_NEW, 0, colour, attribs, p[0] );
}


void ScriptRecorder::deleteStroke( int index )
{
  if ( m_running )
    m_log->append( m_lastTick, ScriptEntry::OP_DELETE, index );
}

void ScriptRecorder::extendStroke( int index, const Vec2& pt )
{
  if ( m_running )
    m_log->append( m_lastTick, ScriptEntry::OP_EXTEND, index, 0, 0, pt );
}

void ScriptRecorder::moveStroke( int index, const Vec2& pt )
{
  if ( m_running )
    m_log->append( m_lastTick, ScriptEntry::OP_MOVE, index, 0, 0, pt );
}

void ScriptRecorder::activateStroke( int index )
{
  if ( m_running )
    m_log->append( m_lastTick, ScriptEntry::OP_ACTIVATE, index );
}




void ScriptPlayer::start( const ScriptLog* log, Scene* scene )
{
  m_playing = true;
  m_log = log;
  m_index = 0;
  m_lastTick = 0;
  m_scene = scene;
  printf("start playback: %d events\n",m_log->size());
}


void ScriptPlayer::stop()  
{ 
  m_playing = false; 
  m_log = NULL;
}

bool ScriptPlayer::isRunning()
{
  return m_log && m_log->size() > 0 && m_playing; 
}

void ScriptPlayer::tick() 
{
  if ( m_playing ) {
    m_lastTick++;

    while ( m_index < m_log->size()
	 && m_log->at(m_index).t <= m_lastTick ) {
      const ScriptEntry& e = m_log->at(m_index);
      switch (e.op) {
      case ScriptEntry::OP_NEW:
	m_scene->newStroke( Path(Path()&e.pt), e.arg1, e.arg2 );
	break;
      case ScriptEntry::OP_DELETE:
	m_scene->deleteStroke( m_scene->strokes()[e.stroke] );
	break;
      case ScriptEntry::OP_EXTEND:
	m_scene->extendStroke( m_scene->strokes()[e.stroke], e.pt );
	break;
      case ScriptEntry::OP_MOVE:
	m_scene->moveStroke( m_scene->strokes()[e.stroke], e.pt );
	break;
      case ScriptEntry::OP_ACTIVATE:
	m_scene->activateStroke( m_scene->strokes()[e.stroke] );
	break;
      }
      m_index++;
    }
  }
}


