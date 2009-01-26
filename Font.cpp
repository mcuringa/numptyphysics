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
#include "Font.h"
#include "Canvas.h"
#include "Config.h"

Font::Font( const std::string& file )
{
  std::string fname = Config::findFile(file);
  FILE* f = fopen(fname.c_str(),"rt");
  if ( f ) {
    int c = 'A';
    char buf[1024];
    while ( fgets(buf,1024,f) ) {
      char *s = buf;
      while ( *s && *s != ':' ) s++;
      m_glyphs[c] = Path(++s);
      m_glyphs[c].translate( -m_glyphs[c].bbox().tl );
      c++;
    }
    fclose(f);
  }
}


Vec2 Font::metrics( const std::string& text )
{
  Vec2 m(0,0);
  for ( int i=0; i<text.length();i++ ) {
    if ( m_glyphs[text[i]].size() > 1 ) {
      Rect r = m_glyphs[text[i]].bbox();
      m.x += r.width()+1;
      m.y += r.height();
    }
  }
  return m;
}

void Font::draw( Canvas* canvas, Vec2 pt, const std::string& text, int colour )
{
  for ( int i=0; i<text.length();i++ ) {
    char c = text[i];
    switch ( c ) {
    case 'a'...'z':
      c += 'A' - 'a';
      break;
    case '_': 
      c = ' ';
      break;
    }
    if ( c >= 0 && c < 128 && m_glyphs[c].size() > 1 ) {
      Rect r = m_glyphs[c].bbox();
      Path p = m_glyphs[c];
      p.translate( pt );
      canvas->drawPath( p, colour, false );
      pt.x += r.width()+1;
    } else if ( c==' ' ) {
      pt.x += m_glyphs['E'].bbox().width();
    }
  }
}

Font* Font::rescale( double factor )
{
  Font *f = new Font(*this);
  for ( int i=0; i<128; i++ ) {
    f->m_glyphs[i].scale( factor );
  }
  return f;
}
