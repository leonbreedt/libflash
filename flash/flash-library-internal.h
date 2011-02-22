/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#ifndef __FLASH_LIBRARY_INTERNAL_H__
#define __FLASH_LIBRARY_INTERNAL_H__

#include <glib.h>
#include "npupp.h"

G_BEGIN_DECLS

typedef NPError (*NPInitializeFunc)(NPNetscapeFuncs*, NPPluginFuncs*);
typedef NPError (*NPShutdownFunc)(void);
typedef char*   (*NPGetMIMEDescriptionFunc)(void);
typedef NPError (*NPGetValueFunc)(void *, NPPVariable, void *);
typedef void    (*SPFVoidVoidPFunc)(void *);
typedef void    (*SPFVoidVoidPIntPFunc)(void *, int *);

struct _FlashLibrary {
  GObject  parent;
  GModule *module;
  gchar *path;
  gboolean initialized;

  NPNetscapeFuncs *exports;

  /* Gecko NPAPI static vtable */
  NPInitializeFunc         npf_initialize;
  NPShutdownFunc           npf_shutdown;
  NPGetMIMEDescriptionFunc npf_get_mime_description;
  NPGetValueFunc           npf_get_value;

  /* Gecko NPAPI dynamic vtable */
  NPPluginFuncs            npf_vtable;

  /* ScriptablePeer functions */
  SPFVoidVoidPFunc         spf_play;
  SPFVoidVoidPFunc         spf_stop_play;
  SPFVoidVoidPIntPFunc     spf_is_playing;
  SPFVoidVoidPFunc         spf_release;

  /* Public object properties */
  gchar *description;
};


NPPluginFuncs *flash_library_get_plugin_vtable  (FlashLibrary *library);
void          *flash_library_load_custom_symbol (FlashLibrary *library,
                                                 const gchar *name);

G_END_DECLS

#endif
