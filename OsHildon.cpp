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
#include <mce/mode-names.h>
#include <mce/dbus-names.h>
#include <dbus/dbus-glib.h>
#include <libosso.h>

#include <hildon/hildon-program.h>
//#include <hildon/hildon-file-chooser-dialog.h>
#include <gtk/gtk.h>

#include "Os.h"
#include "Http.h"
#include "Accelerometer.h"
#define Font __FONT_REDEF
#include "Config.h"

#define NP_NAME       "NumptyPhysics"
#define NP_SERVICE    "org.maemo.garage.numptyphysics"
#define NP_OBJECT     "org/maemo/garage/numptyphysics" /* / ?? */
#define NP_INTERFACE  "org.maemo.garage.numptyphysics"
#define NP_VERSION    "1.0"
#define MAX_FILES 32


static void orientation_callback (DBusGProxy *proxy,
				  DBusGProxyCall *call, 
				  void *data);
static gint mime_handler(const gchar *interface,
                         const gchar *method,
                         GArray *arguments,
                         gpointer data,
                         osso_rpc_t *retval);

class OsHildon : public Os, private Accelerometer
{
  GMainContext   *m_gcontext;
  osso_context_t *m_osso;
  DBusGProxy     *m_mce_proxy;
  DBusGProxyCall *m_mce_call;
  bool            m_mce_ok;
  gint            m_mgx, m_mgy, m_mgz;


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
			   mime_handler, NULL) != OSSO_OK) {
      fprintf(stderr, "Failed to set mime callback\n");
    }

    m_mce_proxy = dbus_g_proxy_new_for_name_owner( dbus_g_bus_get(DBUS_BUS_SYSTEM, NULL),
						   "com.nokia.mce",
						   "/com/nokia/mce/request",
						   "com.nokia.mce.request", NULL);
    m_mce_call = NULL;
    m_mce_ok = false;
    m_mgx = m_mgy = m_mgz = 0;
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

  virtual Accelerometer* getAccelerometer()
  {
    return this;
  }

  virtual bool poll( float32& gx, float32& gy, float32& gz )
  {
    if ( m_mce_call ) {
      // without a glib main loop we have to do a blocking wait on
      // call end here.  this means we are lockstepped to 1 dbus
      // roundtrip per poll.
      orientationCallback( m_mce_proxy, m_mce_call );
    } 
    if ( !m_mce_call ) {
      // dispatch a new call.
      m_mce_call = dbus_g_proxy_begin_call( m_mce_proxy,
					    "get_device_orientation",
					    orientation_callback,
					    this, NULL,
					    G_TYPE_INVALID );
    }
    if ( m_mce_ok ) {
      // align with screen coords
      gx = -(float32)m_mgx / 1000.0f;
      gy = -(float32)m_mgy / 1000.0f;
      gz =  (float32)m_mgz / 1000.0f;
      return true;
    }
    return false;
  }

  void orientationCallback (DBusGProxy *proxy, DBusGProxyCall *call)
  {
    if (proxy == m_mce_proxy && call==m_mce_call) {
      gchar *s1, *s2, *s3;

      if (dbus_g_proxy_end_call (proxy, call, NULL,
				 G_TYPE_STRING, &s1,
				 G_TYPE_STRING, &s2,
				 G_TYPE_STRING, &s3,
				 G_TYPE_INT, &m_mgx,
				 G_TYPE_INT, &m_mgy,
				 G_TYPE_INT, &m_mgz,
				 G_TYPE_INVALID)) {
	m_mce_ok = true;
      }
    }
    m_mce_call = NULL;
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


static void orientation_callback (DBusGProxy *proxy,
				  DBusGProxyCall *call, 
				  void *data)
{
  OsHildon* os =(OsHildon*)data;
  os->orientationCallback(proxy,call);
}


static gint mime_handler(const gchar *interface,
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
