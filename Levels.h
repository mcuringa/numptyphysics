#ifndef LEVELS_H
#define LEVELS_H

#include <cstdio>
#include <sstream>
#include "Array.h"

class Levels
{
 public:
  Levels( int numDirs=0, const char** dirs=NULL );
  bool addPath( const char* path );
  bool addLevel( const std::string& file, int rank=-1 );
  int  numLevels();
  const std::string& levelFile( int i ); 
  int  levelSize( int l );
  bool load( int l, void* buf, int buflen );
 private:
  bool scanCollection( std::string& file, int rank );
  struct LevelDesc
  {
  LevelDesc( const std::string& f,int r=0,int i=-1)
  : file(f), index(i), rank(r) {}
    std::string file;
    int         index;
    int         rank;
  };
  Array<LevelDesc*> m_levels;
};

#endif //LEVELS_H
