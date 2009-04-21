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

#ifndef WORKER_H
#define WORKER_H

#include <SDL/SDL.h>

class WorkerBase
{
 public:
  WorkerBase( void (*func)(void*) ) { m_func = func; }  
  void start();
  bool done();

 private:
  void       (*m_func)(void*);
  SDL_Thread  *m_thread;
};

template< typename A, typename B, typename C >
class Worker : public WorkerBase
{
 public:
  typedef void (*F)(A,B,C);

  Worker( F ff, A aa, B bb, C cc )
    : f(ff), a(aa), b(bb), c(cc),
      WorkerBase( worker, this )
    {}
  
  static void worker( void *s)    
  {
    Worker* self = (Worker*)s;
    self->f( self->a, self->b, self->c );
  }
 private:
  F f; A a; B b; C c;
};




#endif //WORKER_H
