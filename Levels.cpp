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

#include <cstring>
#include <sys/types.h>
#include <dirent.h>

#include "Levels.h"
#include "ZipFile.h"
#include "Os.h"

using namespace std;

static int rankFromPath( const string& p, int defaultrank=9999 )
{
  const char *c = p.data();
  size_t i = p.rfind(Os::pathSep);
  if ( i != string::npos ) {
    c += i+1;
    if ( *c++=='L' ){
      int rank=0;
      while ( *c>='0' && *c<='9' ) {
	rank = rank*10 + (*c)-'0';
	c++;
      }
      return rank;
    }
  }
  return defaultrank;
}

Levels::Levels( int numFiles, const char** names )
{
  for ( int d=0;d<numFiles;d++ ) {
    addPath( names[d] );
  }
}

bool Levels::addPath( const char* path )
{
  int len = strlen( path );
  if ( strcasecmp( path+len-4, ".npz" )==0 ) {
    scanCollection( string(path), rankFromPath(path) );
  } else if ( strcasecmp( path+len-4, ".nph" )==0 ) {
    addLevel( path, rankFromPath(path) );
  } else {
    DIR *dir = opendir( path );
    if ( dir ) {
      struct dirent* entry;
      while ( (entry = readdir( dir )) != NULL ) {
	if ( entry->d_name[0] != '.' ) {
	  string full( path );
	  full += "/";
	  full += entry->d_name;
	  //DANGER - recursion may not halt for linked dirs 
	  addPath( full.c_str() );
	}
      }
      closedir( dir );
    } else {
      printf("bogus level path %s\n",path);
    }
  }
  return true;
}

bool Levels::addLevel( const string& file, int rank, int index )
{
  LevelDesc *e = new LevelDesc( file, rank, index );
  for ( int i=0; i<m_levels.size(); i++ ) {
    if ( m_levels[i]->file == file
	 && m_levels[i]->index == index ) {
      printf("addLevel %s already present!\n",file.c_str());
      return false;
    } else if ( m_levels[i]->rank > rank ) {
      printf("addLevel %s at %d\n",file.c_str(),i);
      m_levels.insert(i,e);
      return true;
    }
  }
  printf("top level %s\n",file.c_str());
  m_levels.append( e );
  return true;
}


bool Levels::scanCollection( const std::string& file, int rank )
{
  ZipFile zf(file);
  printf("found collection %s with %d levels\n",file.c_str(),zf.numEntries());
  for ( int i=0; i<zf.numEntries(); i++ ) {
    addLevel( file, rankFromPath(zf.entryName(i),rank), i );
  }
  return false;
}

int Levels::numLevels()
{
  return m_levels.size();
}


int Levels::load( int i, unsigned char* buf, int bufLen )
{
  int l=0;
  if ( i < m_levels.size() ) {
    if ( m_levels[i]->index >= 0 ) {
      ZipFile zf( m_levels[i]->file.c_str() );
      if ( m_levels[i]->index < zf.numEntries() ) {
	unsigned char* d = zf.extract( m_levels[i]->index, &l);
	if ( d && l <= bufLen ) {
	  memcpy( buf, d, l );
	}
      }
    } else {
      FILE *f = fopen( m_levels[i]->file.c_str(), "rt" );
      if ( f ) {
	l = fread( buf, 1, bufLen, f );
	fclose(f);
      }
    }
    return l;
  }
  throw "invalid level index";
  
}

std::string Levels::levelName( int i )
{
  std::string s = "end";
  if ( i < m_levels.size() ) {
    if ( m_levels[i]->index >= 0 ) {
      ZipFile zf( m_levels[i]->file.c_str() );
      s = zf.entryName( m_levels[i]->index );
    } else {
      s = m_levels[i]->file;
    }
  }
  size_t j = s.rfind(Os::pathSep);
  size_t k = s.rfind('.');
  return s.substr(j+1,k-j-1);
}

#if 0
const std::string& Levels::levelFile( int i )
{
  if ( i < m_levels.size() ) {
    return m_levels[i]->file;
  }
  throw "invalid level index";
}
#endif


int Levels::findLevel( const char *file )
{
  for ( int i=0; i<m_levels.size(); i++ ) {
    if ( m_levels[i]->file == file ) {
      return i;
    }
  }
  return -1;
}


