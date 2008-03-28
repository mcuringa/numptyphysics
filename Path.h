#ifndef PATH_H
#define PATH_H

#include "Common.h"
#include "Array.h"


class Segment
{
public:
  Segment( const Vec2& p1, const Vec2& p2 )
    : m_p1(p1), m_p2(p2) {}
  float distanceTo( const Vec2& p );
private:
  Vec2 m_p1, m_p2;
};


class Path : public Array<Vec2>
{
public:
  Path();
  Path( int n, Vec2* p );

  void makeRelative();
  Path& translate(const Vec2& xlate);
  Path& rotate(const b2Mat22& rot);
  Path& scale(float factor);

  inline Vec2& origin() { return at(0); }

  inline Path& operator&(const Vec2& other) 
  {
    append(other);
    return *this; 
  }
  
  inline Path& operator&(const b2Vec2& other) 
  {
    append(Vec2(other));
    return *this; 
  }
  
  inline Path operator+(const Vec2& p) const
  {
    Path r( *this );
    return r.translate( p );
  }

  inline Path operator-(const Vec2& p) const
  {
    Path r( *this );
    Vec2 n( -p.x, -p.y );
    return r.translate( n );
  }

  inline Path operator*(const b2Mat22& m) const
  {
    Path r( *this );
    return r.rotate( m );
  }

  inline Path& operator+=(const Vec2& p) 
  {
    return translate( p );
  }

  inline Path& operator-=(const Vec2& p) 
  {
    Vec2 n( -p.x, -p.y );
    return translate( n );
  }

  inline int   numPoints() const { return size(); }
  inline const Vec2& point(int i) const { return at(i); }
  inline Vec2& point(int i) { return at(i); }
  inline Vec2& first() { return at(0); }
  inline Vec2& last() { return at(size()-1); }

  void simplify( float threshold );
  Rect bbox() const;

 private:
  void simplifySub( int first, int last, float threshold, bool* keepflags );  
};

#endif //PATH_H
