
#ifdef USE_HILDON
#include <stdio.h>
#include <glibconfig.h>
#include <glib/gmacros.h>
#include <libosso.h>

#include "Hildon.h"

#define NP_NAME       "NumptyPhysics"
#define NP_SERVICE    "org.maemo.garage.numptyphysics"
#define NP_OBJECT     "org/maemo/garage/numptyphysics" /* / ?? */
#define NP_INTERFACE  "org.maemo.garage.numptyphysics"
#define NP_VERSION    "1.0"
#define MAX_FILES 128

static struct HildonState {
  GMainContext   *gcontext;
  osso_context_t *osso;
  int   numFiles;
  char* files[MAX_FILES];
} g_state = {NULL,0};


static gint dbus_handler(const gchar *interface,
                         const gchar *method,
                         GArray *arguments,
                         gpointer data,
                         osso_rpc_t *retval)
{
  if (arguments == NULL) {
    return OSSO_OK;
  }

  if (g_ascii_strcasecmp(method, "mime_open") == 0) {
    for(gint i = 0; i < arguments->len; ++i) {
      osso_rpc_t val = g_array_index(arguments, osso_rpc_t, i);
      if (val.type == DBUS_TYPE_STRING && val.value.s != NULL) {
	fprintf(stderr,"hildon mime open \"%s\"\n",val.value.s);
	if ( g_state.numFiles < MAX_FILES ) {
	  g_state.files[g_state.numFiles++] = g_strdup(val.value.s+7);
	}
	//g_idle_add(addfile, (gpointer)g_strdup(val.value.s));
      }
    }
  }

  return OSSO_OK;
}


Hildon::Hildon()
{
  if ( g_state.gcontext ) {
    throw "gmainloop already initialised";
  } else {
    g_state.gcontext = g_main_context_new();
  }
  if ( g_state.osso ) {
    throw "hildon already initialised";
  } else {
    g_state.osso = osso_initialize(NP_NAME, NP_VERSION, FALSE, g_state.gcontext);
    if (g_state.osso == NULL) {
      fprintf(stderr, "Failed to initialize libosso\n");
      return;
    }
    
    /* Set dbus handler to get mime open callbacks */
    if ( osso_rpc_set_cb_f(g_state.osso,
			   NP_SERVICE,
			   NP_OBJECT,
			   NP_INTERFACE,
			   dbus_handler, NULL) != OSSO_OK) {
      fprintf(stderr, "Failed to set mime callback\n");
      return;
    }
  }
}

Hildon::~Hildon()
{
  if ( g_state.osso ) {
    osso_deinitialize( g_state.osso );
  }
  if ( g_state.gcontext ) {
    g_main_context_unref( g_state.gcontext );
  }
}


void Hildon::poll()
{
  if ( g_main_context_iteration( g_state.gcontext, FALSE ) ) {
    fprintf(stderr, "Hildon::poll event!\n");
  }
}

char *Hildon::getFile()
{
  if ( g_state.numFiles > 0 ) {
    return g_state.files[--g_state.numFiles];
  }
  return NULL;
}

#endif //USE_HILDON
