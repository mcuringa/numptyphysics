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

#ifdef USE_HILDON

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <glib-object.h>
#include <glibconfig.h>
#include <glib/gmacros.h>
#include <libosso.h>

#include <hildon/hildon-program.h>
//#include <hildon/hildon-file-chooser-dialog.h>
#include <gtk/gtk.h>

#include "Os.h"
#include "Http.h"
#define Font __FONT_REDEF
#include "Config.h"

#define NP_NAME       "NumptyPhysics"
#define NP_SERVICE    "org.maemo.garage.numptyphysics"
#define NP_OBJECT     "org/maemo/garage/numptyphysics" /* / ?? */
#define NP_INTERFACE  "org.maemo.garage.numptyphysics"
#define NP_VERSION    "1.0"
#define MAX_FILES 32


static gint dbus_handler(const gchar *interface,
                         const gchar *method,
                         GArray *arguments,
                         gpointer data,
                         osso_rpc_t *retval);

class OsHildon : public Os
{
  GMainContext   *m_gcontext;
  osso_context_t *m_osso;

 public:
  int             m_numFiles;
  char*           m_files[MAX_FILES];

  OsHildon()
  {
    g_type_init();
    m_gcontext = g_main_context_new();

    m_osso = osso_initialize(NP_NAME, NP_VERSION, FALSE, m_gcontext);
    if (m_osso == NULL) {
      fprintf(stderr, "Failed to initialize libosso\n");
      return;
    }
    
    /* Set dbus handler to get mime open callbacks */
    if ( osso_rpc_set_cb_f(m_osso,
			   NP_SERVICE,
			   NP_OBJECT,
			   NP_INTERFACE,
			   dbus_handler, NULL) != OSSO_OK) {
      fprintf(stderr, "Failed to set mime callback\n");
      return;
    }
  }

  ~OsHildon()
  {
    if ( m_osso ) {
      osso_deinitialize( m_osso );
    }
    if ( m_gcontext ) {
      g_main_context_unref( m_gcontext );
    }
  }

  virtual void poll() 
  {
    if ( g_main_context_iteration( m_gcontext, FALSE ) ) {
      fprintf(stderr, "Hildon::poll event!\n");
    }
  }

  virtual char *getLaunchFile() 
  {
    if ( m_numFiles > 0 ) {
      return m_files[--m_numFiles];
    }
    return NULL;
  }

  virtual bool openBrowser( const char* url )
  {
    if ( url && strlen(url) < 200 ) {
      char buf[256];
      snprintf(buf,256,"xdg-open %s",url);
      if ( system( buf ) == 0 ) {
	return true;
      }
    }
    return false;
  }

  virtual char* saveDialog( const char* path )
  {
#if 0
    static char buf[256];
    GtkWidget *dialog = hildon_file_chooser_dialog_new(NULL,GTK_FILE_CHOOSER_ACTION_SAVE);
    gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), path);
    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK) {
      gchar *name;
      name = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
      strncpy( buf, name, 256 );
      g_print("Saving as %s\n", name);
      g_free(name);
    }    
    return buf;    
#else
    return NULL;
#endif
  }

};


static gint dbus_handler(const gchar *interface,
                         const gchar *method,
                         GArray *arguments,
                         gpointer data,
                         osso_rpc_t *retval)
{
  OsHildon* os = (OsHildon*)data;
  if (arguments == NULL) {
    return OSSO_OK;
  }

  if (g_ascii_strcasecmp(method, "mime_open") == 0) {
    for(unsigned i = 0; i < arguments->len; ++i) {
      osso_rpc_t val = g_array_index(arguments, osso_rpc_t, i);
      if (val.type == DBUS_TYPE_STRING && val.value.s != NULL) {
	char *f = NULL;
	fprintf(stderr,"hildon mime open \"%s\"\n",val.value.s);
	if ( strncmp(val.value.s,"file://",7)==0 
	     && os->m_numFiles < MAX_FILES ) {
	  f = val.value.s+7;
	} else if ( ( strncmp(val.value.s,"http://",7)==0 
		      || strncmp(val.value.s,"nptp://",7)==0 ) ) {
	  Http h;
	  std::string filename(val.value.s+7);
	  if ( filename.rfind('/') >= 0 ) {
	    filename = filename.substr( filename.rfind('/')+1 );
	  }
	  std::string filepath = Config::userDataDir() + Os::pathSep;
	  if ( h.get( val.value.s+7, HTTP_TEMP_FILE ) ) {
	    f = HTTP_TEMP_FILE;
	  }
	}
	if ( f ) {
	  if ( os->m_files[os->m_numFiles] ) {
	    g_free(os->m_files[os->m_numFiles]);
	  }
	  os->m_files[os->m_numFiles++] = g_strdup( f );
	}
      }
    }
  }

  return OSSO_OK;
}


Os* Os::get()
{
  static OsHildon os;
  return &os;
}

const char Os::pathSep = '/';

int main(int argc, char** argv)
{
  gtk_init(&argc, &argv);
  npmain(argc,argv);
}

#endif
