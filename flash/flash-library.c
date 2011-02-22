/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#include <string.h>

#include "flash-common.h"
#include "flash-npapi.h"
#include "flash-library.h"
#include "flash-library-internal.h"
#include "flash-file-internal.h"

#define FLASH_LIBRARY_UA "Mozilla/5.0 (X11; U; Linux i686; en-US; rv:1.7.5) " \
                         "Gecko/20041116 Firefox/1.0" 

/* _FlashLibrary is defined in flash-library-internal.h */

struct _FlashLibraryClass {
  GObjectClass parent;
};

enum
{
  PROPERTY_DESCRIPTION = 1,
};

static void flash_library_class_init (FlashLibraryClass *);
static void flash_library_init       (FlashLibrary *);
static void flash_library_finalize   (GObject *);

static void flash_library_set_property (GObject *object,
                                       guint param_id,
                                       const GValue *value,
                                       GParamSpec *pspec);

static void flash_library_get_property (GObject *object,
                                       guint param_id,
                                       GValue *value,
                                       GParamSpec *pspec);


static NPError     flash_npapi_geturl         (NPP, const char *, const char *);
static NPError     flash_npapi_posturl        (NPP, const char *, const char *,
                                               uint32, const char *, NPBool);
static NPError     flash_npapi_requestread    (NPStream *, NPByteRange *);
static NPError     flash_npapi_newstream      (NPP, NPMIMEType,
                                               const char *, NPStream **);

static int32       flash_npapi_write          (NPP, NPStream *, int32, void *);
static NPError     flash_npapi_destroystream  (NPP, NPStream *, NPReason);
static void        flash_npapi_status         (NPP, const char *);
static const char* flash_npapi_useragent      (NPP);
static void*       flash_npapi_memalloc       (uint32);
static void        flash_npapi_memfree        (void *);
static uint32      flash_npapi_memflush       (uint32);
static void        flash_npapi_reloadplugins  (NPBool);
static JRIEnv *    flash_npapi_getjavaenv     (void);
static jref        flash_npapi_getjavapeer    (NPP);
static NPError     flash_npapi_geturlnotify   (NPP, const char *, const char *,
                                               void *);
static NPError     flash_npapi_posturlnotify  (NPP, const char *, const char *,
                                               uint32, const char *, NPBool, void *);
static NPError     flash_npapi_getvalue       (NPP, NPPVariable, void *);
static NPError     flash_npapi_setvalue       (NPP, NPPVariable, void *);

static void        flash_npapi_invalidaterect   (NPP, NPRect *);
static void        flash_npapi_invalidateregion (NPP, NPRegion);
static void        flash_npapi_forceredraw      (NPP);

GType
flash_library_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      sizeof (FlashLibraryClass),
      NULL,
      NULL,
      (GClassInitFunc) flash_library_class_init,
      NULL,
      NULL,
      sizeof (FlashLibrary),
      0,
      (GInstanceInitFunc) flash_library_init,
      NULL
    };
    
    type = g_type_register_static (G_TYPE_OBJECT, "FlashLibrary", &info, 0);
  }

  return type;
}

FlashLibrary *
flash_library_new (const gchar *path, GError **error)
{
  FlashLibrary *library;
  GModule *module;
  NPNetscapeFuncs *exports;
  NPError nperr;
  const char *str;
  gchar *canon_path;
  const gchar *exts[] = { ".so", NULL };

  canon_path = flash_canonicalize_path (path);
  if (!canon_path)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS, "%s",
                 "Invalid library file");
    return NULL;
  }
  if (!flash_is_valid_file (canon_path, exts, error))
  {
    g_free (canon_path);
    return NULL;
  }

  module = g_module_open (canon_path, G_MODULE_BIND_LAZY);
  if (!module)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_INIT_FAILED, "%s",
                 g_module_error ());
    g_free (canon_path);
    return NULL;
  }
  library = g_object_new (FLASH_TYPE_LIBRARY, NULL);
  library->module = module;
  library->path = canon_path;

  if ((!g_module_symbol (module, "NP_Initialize",
                         (gpointer *)&library->npf_initialize)) ||
      (!g_module_symbol (module, "NP_Shutdown",
                         (gpointer *)&library->npf_shutdown)) ||
      (!g_module_symbol (module, "NP_GetMIMEDescription",
                         (gpointer *)&library->npf_get_mime_description)) ||
      (!g_module_symbol (module, "NP_GetValue",
                         (gpointer *)&library->npf_get_value)))
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_INIT_FAILED, "%s",
                 g_module_error ());
    g_object_unref (library);
    return NULL;
  }

  /* These functions are a bit of a hack, so if they fail, no big deal, we
   * just lose some functionality 
   */
  library->spf_play = NULL;
  library->spf_stop_play = NULL;
  library->spf_is_playing = NULL;
  library->spf_release = NULL;
  g_module_symbol (module, "ScriptablePeer_Play",
                   (gpointer *)&library->spf_play);
  g_module_symbol (module, "ScriptablePeer_StopPlay",
                   (gpointer *)&library->spf_stop_play);
  g_module_symbol (module, "ScriptablePeer_IsPlaying",
                   (gpointer *)&library->spf_is_playing);
  g_module_symbol (module, "ScriptablePeer_release",
                   (gpointer *)&library->spf_release);

  exports = g_new (NPNetscapeFuncs, 1);
  memset (exports, 0, sizeof(NPNetscapeFuncs));
  exports->size = sizeof(NPNetscapeFuncs);
  exports->version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

  exports->geturl = flash_npapi_geturl;
  exports->posturl = flash_npapi_posturl;
  exports->requestread = flash_npapi_requestread;
  exports->newstream = flash_npapi_newstream;
  exports->write = flash_npapi_write;
  exports->destroystream = flash_npapi_destroystream;
  exports->status = flash_npapi_status;
  exports->uagent = flash_npapi_useragent;
  exports->memalloc = flash_npapi_memalloc;
  exports->memfree = flash_npapi_memfree;
  exports->memflush = flash_npapi_memflush;
  exports->reloadplugins = flash_npapi_reloadplugins;
  exports->getJavaEnv = flash_npapi_getjavaenv;
  exports->getJavaPeer = flash_npapi_getjavapeer;
  exports->geturlnotify = flash_npapi_geturlnotify;
  exports->posturlnotify = flash_npapi_posturlnotify;
  exports->getvalue = flash_npapi_getvalue;
  exports->setvalue = flash_npapi_setvalue;
  exports->invalidaterect = flash_npapi_invalidaterect;
  exports->invalidateregion = flash_npapi_invalidateregion;
  exports->forceredraw = flash_npapi_forceredraw;
  library->exports = exports;

  nperr = library->npf_initialize (library->exports, &library->npf_vtable);
  if (nperr != NPERR_NO_ERROR)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_INIT_FAILED,
                 "Failed to initialize Flash library: %d", nperr);
    g_object_unref (library);
    return NULL;
  }
  library->initialized = TRUE;

  library->npf_get_value (NULL, NPPVpluginDescriptionString, &str);
  library->description = g_strdup (str);

  return library;
}

NPPluginFuncs *
flash_library_get_plugin_vtable (FlashLibrary *library)
{
  return &library->npf_vtable;
}

void *
flash_library_load_custom_symbol (FlashLibrary *library, const gchar *name)
{
  void *sym;

  if (g_module_symbol (library->module, name, &sym))
    return sym;
  return NULL;
}

static void
flash_library_class_init (FlashLibraryClass *klass)
{
  GParamSpec *description_param;
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  description_param = g_param_spec_string ("description",
                                           "library description",
                                           "description (name, version) of the Flash library",
                                           NULL,
                                           G_PARAM_READABLE);
  
  object_class->set_property = flash_library_set_property;
  object_class->get_property = flash_library_get_property;
  object_class->finalize = flash_library_finalize;

  g_object_class_install_property (object_class, PROPERTY_DESCRIPTION, description_param);
}

static void
flash_library_init (FlashLibrary *lib)
{
  lib->module = NULL;
  lib->exports = NULL;
  lib->path = NULL;
  lib->initialized = FALSE;

  lib->description = NULL;
}

static void
flash_library_finalize (GObject *object)
{
  FlashLibrary *library;

  library = FLASH_LIBRARY (object);

  if (library->module)
  {
    if (library->initialized)
      library->npf_shutdown ();
    g_module_close (library->module);
  }

  if (library->exports)
    g_free (library->exports);

  if (library->path)
    g_free (library->path);

  if (library->description)
    g_free (library->description);

  library->module = NULL;
  library->exports = NULL;
  library->path = NULL;
  library->description = NULL;
}

static void
flash_library_set_property (GObject *object,
                            guint param_id,
                            const GValue *value,
                            GParamSpec *pspec)
{
  FlashLibrary *library;

  library = FLASH_LIBRARY (object);
  switch (param_id)
  {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

static void
flash_library_get_property (GObject *object,
                            guint param_id,
                            GValue *value,
                            GParamSpec *pspec)
{
  FlashLibrary *library;

  library = FLASH_LIBRARY (object);
  switch (param_id)
  {
    case PROPERTY_DESCRIPTION:
      g_value_set_string (value, library->description);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
  }
}

/* --- Emulated Gecko API --- */

static NPError
flash_npapi_geturl (NPP instance, const char *url, const char *window)
{
  DEBUG("NPN_GetURL: url='%s' window='%s'", url, window ? window : "NULL");
  return flash_npapi_geturlnotify (instance, url, window, NULL);
}

static NPError
flash_npapi_posturl (NPP instance, const char *url, const char *window, uint32 len,
                     const char *buf, NPBool file)
{
  DEBUG("NPN_PostURL: UNIMPLEMENTED");
  return NPERR_GENERIC_ERROR;
}

static NPError
flash_npapi_requestread (NPStream *stream, NPByteRange *range)
{
  DEBUG("NPN_RequestRead: UNIMPLEMENTED");
  return NPERR_GENERIC_ERROR;
}

static NPError
flash_npapi_newstream (NPP instance, NPMIMEType type, const char *window, NPStream **stream)
{
  DEBUG("NPN_NewStream: UNIMPLEMENTED");
  return NPERR_GENERIC_ERROR;
}

static int32
flash_npapi_write (NPP instance, NPStream *stream, int32 len, void *buffer)
{
  DEBUG("NPN_Write: UNIMPLEMENTED");
  return 0;
}

static NPError
flash_npapi_destroystream (NPP instance, NPStream *stream, NPReason reason)
{
  DEBUG("NPN_DestroyStream: UNIMPLEMENTED");
  return NPERR_GENERIC_ERROR;
}

static void
flash_npapi_status (NPP instance, const char *message)
{
  DEBUG("NPN_Status: message='%s'", message);
}

static const char*
flash_npapi_useragent(NPP instance)
{
  DEBUG("NPN_UserAgent: ua='%s'\n", FLASH_LIBRARY_UA);
  return FLASH_LIBRARY_UA;
}

static void*
flash_npapi_memalloc(uint32 size)
{
	void *ptr;

	ptr = g_malloc (size);
  DEBUG("NPN_MemAlloc: ptr=%p size=%d", ptr, size);
  return ptr;
}

static void
flash_npapi_memfree(void* ptr)
{
  DEBUG("NPN_MemFree: ptr=%p", ptr);
  g_free (ptr);
}

static uint32
flash_npapi_memflush(uint32 size)
{
  DEBUG("NPN_MemFlush: size=%d", size);
  return 0;
}

static void
flash_npapi_reloadplugins (NPBool reloadPages)
{
  DEBUG("NPN_ReloadPlugins: UNIMPLEMENTED");
}

static JRIEnv *
flash_npapi_getjavaenv (void)
{
  DEBUG("NPN_GetJavaEnv: UNIMPLEMENTED");
  return NULL;
}

static jref 
flash_npapi_getjavapeer (NPP instance)
{
  DEBUG("NPN_GetJavaPeer: UNIMPLEMENTED");
  return 0;
}

static NPError
flash_npapi_geturlnotify (NPP instance, const char *url, const char *window,
                          void *user_data)
{
  DEBUG("NPN_GetURLNotify: url='%s' window='%s' notifyData=%p", url, window ? window : "NULL", user_data);
  if (user_data)
    flash_file_set_notify ((FlashFile *)instance->ndata, url, user_data);
  return NPERR_NO_ERROR;
}

static NPError
flash_npapi_posturlnotify (NPP instance, const char *url, const char *target, uint32 len,
                           const char *buf, NPBool file, void *notifyData)
{
  DEBUG("NPN_PostURLNotify: UNIMPLEMENTED");
  return NPERR_NO_ERROR;
}

static NPError
flash_npapi_getvalue (NPP instance, NPPVariable variable, void *value)
{
  DEBUG("NPN_GetValue: variable=%d\n", variable);
  switch (variable)
   {
      case NPNVxDisplay:
         return NPERR_INVALID_PARAM;
      case NPNVxtAppContext:
         return NPERR_INVALID_PARAM;
      case NPNVjavascriptEnabledBool:
         return NPERR_INVALID_PARAM;
      case NPNVasdEnabledBool:
         return NPERR_INVALID_PARAM;
      case NPNVisOfflineBool:
         return NPERR_INVALID_PARAM;
      default:
         return NPERR_INVALID_PARAM;
   }
}

static NPError 
flash_npapi_setvalue (NPP instance, NPPVariable variable, void *value)
{
  DEBUG("NPN_SetValue: variable=%d value=%p", variable, value);
  return NPERR_NO_ERROR;
}

static void
flash_npapi_invalidaterect (NPP instance, NPRect *rect)
{
  DEBUG("NPN_InvalidateRect: UNIMPLEMENTED");
}

static void
flash_npapi_invalidateregion (NPP instance, NPRegion region)
{
  DEBUG("NPN_InvalidateRegion: UNIMPLEMENTED");
}

static void
flash_npapi_forceredraw (NPP instance)
{
  DEBUG("NPN_ForceRedraw: UNIMPLEMENTED");
}
