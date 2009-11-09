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

#include "Dialogs.h"
#include "Ui.h"
#include "Canvas.h"
#include "Font.h"
#include "Config.h"
#include "Game.h"
#include "Scene.h"



////////////////////////////////////////////////////////////////


struct MenuPage : public Panel
{
  MenuPage(bool closeable=false)
  {
    m_transparent = true;
    Box *vbox = new VBox();
    m_content = new Panel();
    vbox->add( m_content, 100, 1 );
    add(vbox);
    fitToParent(true);
  }
  const char* name() {return "MenuPage";}
  Panel *m_content;
};

class LevelSelector : public MenuPage
{
  static const int THUMB_COUNT = 18;
  GameControl* m_game;
  Levels* m_levels;
  int m_collection;
  int m_dispbase;
  int m_dispcount;
  IconButton* m_thumbs[THUMB_COUNT];
  ScrollArea* m_scroll;
public:
  LevelSelector(GameControl* game)
    : m_game(game),
      m_levels(game->m_levels),
      m_dispbase(0),
      m_dispcount(0),
      m_collection(m_game->m_level)
  {
    m_scroll = new ScrollArea();
    m_scroll->fitToParent();
    m_scroll->fitToParent(true);
    m_scroll->virtualSize(Vec2(SCREEN_WIDTH,SCREEN_HEIGHT));

    m_content->add(m_scroll,0,0);
    fitToParent(true);
    setCollection(2);
  }
  void setCollection(int c)
  {
    if (c < 0 || c >=m_levels->numCollections()) {
      return;
    }    
    m_collection = c;
    m_dispbase = 0;
    m_dispcount = m_levels->collectionSize(c);
    m_scroll->virtualSize(Vec2(SCREEN_WIDTH,50+(SCREEN_HEIGHT/ICON_SCALE_FACTOR+40)*((m_dispcount+2)/3)));

    m_scroll->empty();
    Box *vbox = new VBox();
    vbox->add( new Spacer(),  10, 0 );
    Box *hbox = new HBox();
    hbox->add( new Spacer(),  0, 1 );
    hbox->add( new Button("<<",Event::PREVIOUS), BUTTON_WIDTH, 0 );
    Label *title = new Label(m_levels->collectionName(c));
    title->transparent(false);
    hbox->add( title, BUTTON_WIDTH, 4 );
    hbox->add( new Button(">>",Event::NEXT), BUTTON_WIDTH, 0 );
    hbox->add( new Spacer(),  0, 1 );
    vbox->add( hbox, 32, 0 );
    vbox->add( new Spacer(),  10, 0 );

    hbox = new HBox();
    hbox->add( new Spacer(),  0, 1 );
    int accumw = 0;
    for (int i=0; i<m_dispcount; i++) {
      accumw += SCREEN_WIDTH / ICON_SCALE_FACTOR;
      if (accumw >= SCREEN_WIDTH) {
	vbox->add(hbox, SCREEN_HEIGHT/ICON_SCALE_FACTOR+30, 4);
	hbox = new HBox();
	hbox->add( new Spacer(),  0, 1 );
	accumw = SCREEN_WIDTH / ICON_SCALE_FACTOR;
      }
      m_thumbs[i] = new IconButton("--","",Event(Event::SELECT,i));
      m_thumbs[i]->font(Font::blurbFont());
      m_thumbs[i]->transparent(true);
      hbox->add( m_thumbs[i],  SCREEN_WIDTH / ICON_SCALE_FACTOR, 0 );
      hbox->add( new Spacer(), 0, 1 );
    }
    vbox->add(hbox, SCREEN_HEIGHT/ICON_SCALE_FACTOR+30, 4);
    vbox->add( new Spacer(),  10, 10 );
    m_scroll->add(vbox,0,0);

    for (int i=0; i<THUMB_COUNT && i+m_dispbase<m_dispcount; i++) {
      fprintf(stderr,"creating thumb\n");
      Canvas temp( SCREEN_WIDTH, SCREEN_HEIGHT );
      Scene scene( true );
      unsigned char buf[64*1024];
      int level = m_levels->collectionLevel(c,i);
      int size = m_levels->load( level, buf, sizeof(buf) );
      if ( size && scene.load( buf, size ) ) {
	scene.draw( temp, FULLSCREEN_RECT );
	m_thumbs[i]->text( m_levels->levelName(level) );
	m_thumbs[i]->canvas( temp.scale( ICON_SCALE_FACTOR ) );
	fprintf(stderr,"thumb done\n");
      }
    }
  }
  bool onEvent(Event& ev)
  {
    switch (ev.code) {
    case Event::PREVIOUS:
      setCollection(m_collection-1);
      return true;
    case Event::NEXT:
      setCollection(m_collection+1);
      return true;
    case Event::SELECT:
      m_game->gotoLevel(m_levels->collectionLevel(m_collection,m_dispbase+ev.x));
      Event closeEvent(Event::CLOSE);
      m_parent->dispatchEvent(closeEvent);
      return true;
    }
    return MenuPage::onEvent(ev);
  }
};

class HelpPage : public MenuPage
{
public:
  HelpPage()
  {
    Box *vbox = new VBox();
    Widget *text = new Label("no help here");
    text->transparent(false);
    text->setBg(0xc0c0b0);
    vbox->add( text, 0, 1 );
    vbox->add( new Button("http://numptyphysics.garage.maemo.org",Event::REPLAY), 36, 0 );
    m_content->add(vbox,0,0);
  }
  bool onEvent(Event& ev)
  {
    if (ev.code == Event::REPLAY) {
      OS->openBrowser("http://numptyphysics.garage.maemo.org");
      return true;
    }
    return Panel::onEvent(ev);
  }
};



struct FrontPage : public MenuPage
{
  FrontPage() : MenuPage(true)
  {
    m_content->add( new IconButton("CHOOSE","choose.png",
				   Event(Event::SELECT,1)),
		    Rect(125,100,275,300) );
    m_content->add( new IconButton("PLAY","play.png",
				   Event(Event::SELECT,2)),
		    Rect(325,100,475,300) );
    m_content->add( new IconButton("HELP","help.png",
				   Event(Event::SELECT,3)),
		    Rect(525,100,675,300) );
    fitToParent(true);
  }
};


class MainMenu : public Dialog
{
  GameControl* m_game;
public:
  MainMenu(GameControl* game)
    : Dialog("NUMPTY PHYSICS",Event::UNDO,Event::QUIT),
      m_game(game)    
  {
    content()->add(new FrontPage());
    m_targetPos = Vec2( 0, 0 );
    sizeTo(Vec2(SCREEN_WIDTH,SCREEN_HEIGHT));
  }
  bool onEvent( Event& ev )
  {
    switch (ev.code) {
    case Event::SELECT:
      switch(ev.x) {
      case 1:
	content()->empty();	
	content()->add(new LevelSelector(m_game));
	rightControl()->text("<--");
	rightControl()->event(Event::CANCEL);
	break;
      case 2:
	close();
	break;
      case 3: 
	content()->empty();
	content()->add(new HelpPage());
	rightControl()->text("<--");
	rightControl()->event(Event::CANCEL);
	break;
      }
      return true;
    case Event::CANCEL:
      content()->empty();
      content()->add(new FrontPage());
      rightControl()->text("X");
      rightControl()->event(Event::QUIT);
      return true;
    default:
      break;
    }
    return Dialog::onEvent(ev);
  }
};


Widget* createMainMenu(GameControl* game)
{
  return new MainMenu(game);
}


////////////////////////////////////////////////////////////////


    static const MenuItem playNormalOpts[] = {
      MenuItem("tool:close.png",Event(Event::SELECT,1,-1)),
      MenuItem("style:close.png",Event(Event::SELECT,2,-1)),
      MenuItem("pause:pause.png",Event::PAUSE),
      MenuItem("undo:undo.png",Event::UNDO),
      MenuItem("",Event::NOP)
    };

    static const MenuItem playPausedOpts[] = {
      MenuItem("tool:close.png",Event(Event::SELECT,1,-1)),
      MenuItem("style:close.png",Event(Event::SELECT,2,-1)),
      MenuItem("resume:play.png",Event::PAUSE),
      MenuItem("undo:undo.png",Event::UNDO),
      MenuItem("",Event::NOP)
    };


class OptsPopup : public MenuDialog
{
public:
  OptsPopup() : MenuDialog(this, "", NULL) 
  {
    m_buttonDim = Vec2(90,90);
  }

  virtual Widget* makeButton( MenuItem* item, const Event& ev )
  {
    fprintf(stderr,"PlayOpts::makeButton %s\n",item->text.c_str());
    std::string file = item->text.substr(item->text.find(':')+1); 
    std::string label = item->text.substr(0,item->text.find(':')); 
    return new IconButton(label,file,ev);
  }
};


class PlayOpts : public OptsPopup
{
public:
  PlayOpts(GameControl* game )
  {
    addItems(game->m_paused ? playPausedOpts : playNormalOpts);
    sizeTo(Vec2(140,480));
    moveTo(Vec2(SCREEN_WIDTH,0));
    m_targetPos = Vec2(SCREEN_WIDTH-140,0);
  }
};

Widget* createPlayOpts(GameControl* game )
{
  return new PlayOpts(game);
}


////////////////////////////////////////////////////////////////


static const MenuItem editOpts[] = {
  MenuItem("menu:close.png",Event::MENU),
  MenuItem("reset:reset.png",Event::RESET),
  MenuItem("share:play.png",Event::PAUSE),
  MenuItem("edit:undo.png",Event::UNDO),
  MenuItem("",Event::NOP)
};

class EditOpts : public OptsPopup
{
public:
  EditOpts(GameControl* game )
  {
    addItems(editOpts);
    sizeTo(Vec2(140,480));
    moveTo(Vec2(-140,0));
    m_targetPos = Vec2(0,0);
  }
};

Widget* createEditOpts(GameControl* game )
{
  return new EditOpts(game);
}


////////////////////////////////////////////////////////////////


class ColourButton : public Button
{
public:
  ColourButton(const std::string& s, int c, const Event& ev)
    : Button(s,ev)
  {
    m_bg = c;
  }
};

class ColourDialog : public MenuDialog
{
public:
  ColourDialog( int num, const int* cols ) 
    : MenuDialog(this,"Select Colour"),
      m_colours(cols)					     
  {
    m_columns *= 2;
    m_buttonDim.x /= 2;
    for (int i=0; i<num; i++) {
      addItem( MenuItem("np",Event(Event::SELECT,1,i)) );
    }
  }
  Widget* makeButton( MenuItem* item, const Event& ev )
  {
    return new ColourButton(item->text,m_colours[item->event.y],ev);
  }
  const int* m_colours;
};

Widget* createColourDialog(int n, const int* cols)
{
  return new ColourDialog(n,cols);
}


////////////////////////////////////////////////////////////////



class IconDialog : public MenuDialog
{
public:
  IconDialog( const std::string &title, const MenuItem* items=NULL ) 
    : MenuDialog(this,title)
  {
    // add items here to use correct buttons
    addItems(items);
  }
  Widget* makeButton( MenuItem* item, const Event& ev )
  {
    fprintf(stderr,"IconDialog::makeButton %s\n",item->text.c_str());
    try {
      return new IconButton(item->text,item->text+".png",ev);
    } catch (...) {
      return new Button(item->text,ev);
    }
  }
};

Widget *createIconDialog( const std::string &title, const MenuItem* items )
{
  return new IconDialog( title, items );
}


class NextLevelDialog : public Dialog
{
  GameControl* m_game;
public:
  NextLevelDialog(GameControl* game)
    : Dialog("BRAVO!!!",Event::NOP,Event::MENU),
      m_game(game)
  {
    rightControl()->text("<--");
    char buf[32];
    const GameStats& stats = m_game->stats();
    int time = (stats.endTime - stats.startTime)/1000;
    int h = time/60/60;
    int m = time/60 - h*60;
    int s = time - m*60;

    Box *vbox = new VBox();
    vbox->add(new Spacer(),10,1);
    if (h > 0) {
      sprintf(buf,"time: %dh %dm %ds",m,h,s);
    } else if (m > 0) {
      int m = time/60/1000;
      sprintf(buf,"time: %dm %ds",m,s);
    } else {
      sprintf(buf,"time: %ds",s);
    }
    vbox->add(new Label(buf),20,0);
    sprintf(buf,"%d stroke%s",stats.strokeCount,stats.strokeCount==1?"":"s");
    vbox->add(new Label(buf),20,0);
    if (stats.pausedStrokes) {
      sprintf(buf,"     (%d while paused)",stats.pausedStrokes);
      vbox->add(new Label(buf),20,0);
    }
    sprintf(buf,"%d undo%s",stats.undoCount,stats.undoCount==1?"":"s");
    vbox->add(new Label(buf),20,0);
    vbox->add(new Spacer(),10,1);
 
    Box *hbox2 = new HBox();
    hbox2->add(new Spacer(),20,0);
    hbox2->add(new Button("review",Event::REPLAY),BUTTON_WIDTH,0);
    hbox2->add(new Spacer(),1,1);
    hbox2->add(new Button("again",Event::RESET),BUTTON_WIDTH,0);
    hbox2->add(new Spacer(),1,1);
    hbox2->add(new Button("next",Event::NEXT),BUTTON_WIDTH,0);
    hbox2->add(new Spacer(),20,0);
    vbox->add(hbox2,BUTTON_HEIGHT,0);

    vbox->add(new Spacer(),10,0);
    content()->add(vbox,0,0);
    m_targetPos = Vec2( 150, 70);
    sizeTo(Vec2(500,240));
  }
};


Widget *createNextLevelDialog( GameControl* game )
{
  return new NextLevelDialog(game);
}
