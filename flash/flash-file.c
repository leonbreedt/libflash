/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#include <gdk/gdkx.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "flash-common.h"
#include "flash-npapi.h"
#include "flash-file.h"
#include "flash-file-internal.h"
#include "flash-library-internal.h"
#include "gtk2xtbin.h"

#define MIME_TYPE "application/x-shockwave-flash"
#define PLUGIN_CALL(x, func, args...) (flash_library_get_plugin_vtable((x)->library)->func(args))

struct _FlashFile {
  GObject parent;

  gchar *path;
  FlashLibrary *library;
  NPP instance;
  gboolean npp_instantiated;
  GtkWidget *xt_bin;

  char *notify_url;
  void *notify_data; 

  gboolean is_playing;

  FlashFileEventCallback callback;
  gpointer callback_data;
};

struct _FlashFileClass {
  GObjectClass parent;
};

static void     flash_file_class_init          (FlashFileClass *);
static void     flash_file_init                (FlashFile *);
static void     flash_file_finalize            (GObject *);
static gboolean flash_file_new_attrs           (int *argcp, char ***argnp,
                                                char ***argvp, ...);
static void     flash_file_free_attrs          (int argc, char **argn, char **argv);
static gchar *  flash_file_make_file_url       (const gchar *path);
static gboolean flash_file_send_to_plugin      (FlashFile *file, const gchar *url,
                                               GError **error);
static gboolean flash_file_timer_callback      (gpointer data);
static void *   flash_file_get_script_peer     (FlashFile *file);
static void     flash_file_release_script_peer (FlashFile *file, void *peer);

GType
flash_file_get_type (void)
{
  static GType type = 0;

  if (!type) {
    static const GTypeInfo info = {
      sizeof (FlashFileClass),
      NULL,
      NULL,
      (GClassInitFunc) flash_file_class_init,
      NULL,
      NULL,
      sizeof (FlashFile),
      0,
      (GInstanceInitFunc) flash_file_init,
      NULL
    };
    
    type = g_type_register_static (G_TYPE_OBJECT, "FlashFile", &info, 0);
  }

  return type;
}

FlashFile *
flash_file_new (FlashLibrary *library, const gchar *path,
                FlashFileEventCallback callback,
                gpointer callback_user_data,
                GError **error)
{
  FlashFile *file;
  gchar *canon_path;
  const gchar *exts[] = { ".swf", NULL };

  canon_path = flash_canonicalize_path (path);
  if (!canon_path)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS, "%s",
                 "Invalid Flash file");
    return NULL;
  }
  if (!flash_is_valid_file (canon_path, exts, error))
  {
    g_free (canon_path);
    return NULL;
  }

  file = g_object_new (FLASH_TYPE_FILE, NULL);
  file->path = canon_path;
  file->library = g_object_ref (library);
  file->instance = g_new (NPP_t, 1);

  memset (file->instance, 0, sizeof(NPP_t));
  file->instance->ndata = file;

  file->callback = callback;
  file->callback_data = callback_user_data;

  return file;
}

gboolean
flash_file_play (FlashFile *file, GtkWindow *gtk_window, gboolean loop,
                 GError **error)
{
  int argc;
  char **argn;
  char **argv;
  gint width;
  gint height;
  gint depth;
#define MAX_DIGITS 20
  char width_str[MAX_DIGITS+1];
  char height_str[MAX_DIGITS+1];
  gchar *file_url;
  gboolean ret;
  GtkWidget *xt_bin;
  GdkWindow *window;
  NPError nperr;
  NPWindow npwin;
  NPSetWindowCallbackStruct npws;
  gboolean npp_window_set;

  if (file->is_playing)
    return FALSE;

  argc = -1;
  argn = argv = NULL;
  width = height = depth = -1;
  memset (width_str, 0, sizeof(width_str));
  memset (height_str, 0, sizeof(height_str));
  file_url = NULL;
  ret = FALSE;
  xt_bin = NULL;
  window = NULL;
  nperr = NPERR_GENERIC_ERROR;
  memset (&npwin, 0, sizeof(npwin));
  memset (&npws, 0, sizeof(npws));
  npp_window_set = FALSE;

  window = GTK_WIDGET(gtk_window)->window;
  g_assert (window != NULL);
  gdk_window_get_geometry (window, NULL, NULL, &width, &height, &depth);

  file_url = flash_file_make_file_url (file->path);

  argc = -1;
  argn = argv = NULL;
  snprintf (width_str, sizeof(width_str), "%d", width);
  snprintf (height_str, sizeof(height_str), "%d", height);
  if (loop) 
  {
    flash_file_new_attrs (&argc, &argn, &argv, 
      "SRC", file_url,
      "TYPE", MIME_TYPE,
      "WIDTH", width_str,
      "HEIGHT", height_str,
      "LOOP", "true",
      NULL);
  }
  else
  {
    flash_file_new_attrs (&argc, &argn, &argv, 
      "SRC", file_url,
      "TYPE", MIME_TYPE,
      "WIDTH", width_str,
      "HEIGHT", height_str,
      "LOOP", "false",
      NULL);
  }

  nperr = PLUGIN_CALL(file, newp,
    MIME_TYPE,
    file->instance,
    NP_EMBED,
    argc,
    argn,
    argv,
    NULL);

  if (nperr != NPERR_NO_ERROR)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_PLAY, "%s",
                 "Failed to create playback instance");
    file->npp_instantiated = FALSE;
    goto err_out;
  }
  else
  {
    file->npp_instantiated = TRUE;
  }

  /* Create plugin window */

  xt_bin = gtk_xtbin_new (window, NULL);
  if (!xt_bin)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_PLAY, "%s",
                 "Failed to create playback container");
    goto err_out;
  }
  gtk_widget_show (xt_bin);
  gdk_flush ();

  npwin.window = (void *)GTK_XTBIN (xt_bin)->xtwindow;
  npwin.x = 0;
  npwin.y = 0;
  npwin.width = width;
  npwin.height = height;
  npwin.type = NPWindowTypeWindow;

  npws.type = NP_SETWINDOW;
  npws.depth = gdk_window_get_visual (window)->depth;
  npws.display = GTK_XTBIN (xt_bin)->xtdisplay;
  npws.visual = GDK_VISUAL_XVISUAL (gdk_window_get_visual (window));
  npws.colormap = GDK_COLORMAP_XCOLORMAP (gdk_window_get_colormap (window));

  npwin.ws_info = (void *)&npws;

  XFlush (npws.display);

  gtk_xtbin_resize (xt_bin, width, height);

  /* Plugin, show thyself */

  nperr = PLUGIN_CALL (file, setwindow, file->instance, &npwin);
  if (nperr != NPERR_NO_ERROR)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_PLAY, "%s",
                 "Failed to set plugin window");
    goto err_out;
  }
  npp_window_set = TRUE;

  /* Stream data to plugin */

  if (!flash_file_send_to_plugin (file, file_url, error))
    goto err_out;

  /* Avoid unexpected surprises when we try to release the ref later */
  g_object_ref (xt_bin);

  file->xt_bin = xt_bin;
  file->is_playing = TRUE;

  if (!loop && file->callback)
  {
    /* Ugly, but the Flash plugin has no means for us to register a callback
     * to be called when it finishes playback. */
    g_timeout_add (25, flash_file_timer_callback, file);
  }

  goto out;
err_out:
  ret = FALSE;
  if (npp_window_set)
    PLUGIN_CALL (file, setwindow, file->instance, NULL);
  if (xt_bin)
    gtk_widget_destroy (xt_bin);
  if (file->npp_instantiated)
  {
    PLUGIN_CALL(file, destroy, file->instance, NULL);
    file->npp_instantiated = FALSE;
  }
out:
  if (argc > 0)
    flash_file_free_attrs (argc, argn, argv);
  if (file_url)
    g_free (file_url);
  return ret;
}

gboolean
flash_file_is_playing (FlashFile *file)
{
  void *peer;
  int playing;

  if (!file->library->spf_is_playing)
    return file->is_playing;
  peer = flash_file_get_script_peer (file);
  if (!peer)
    return file->is_playing;
  playing = 0xdeadbabe;
  file->library->spf_is_playing (peer, &playing);
	g_assert (playing != 0xdeadbabe);
  flash_file_release_script_peer (file, peer);
  return (gboolean) playing;
}

gboolean
flash_file_pause (FlashFile *file)
{
  void *peer;

  if (!file->library->spf_stop_play)
    return FALSE;
  peer = flash_file_get_script_peer (file);
  if (!peer)
    return FALSE;
  file->library->spf_stop_play (peer);
  flash_file_release_script_peer (file, peer);
  return TRUE;
}

gboolean
flash_file_resume (FlashFile *file)
{
  void *peer;

  if (!file->library->spf_play)
    return FALSE;
  peer = flash_file_get_script_peer (file);
  if (!peer)
    return FALSE;
  file->library->spf_play (peer);
  flash_file_release_script_peer (file, peer);
  return TRUE;
}

gboolean
flash_file_stop (FlashFile *file)
{
  if (!file->is_playing)
    return FALSE;
  file->is_playing = FALSE;
  if (file->xt_bin)
  {
    gtk_widget_destroy (file->xt_bin);
    file->xt_bin = NULL;
  }
  PLUGIN_CALL (file, setwindow, file->instance, NULL);
  if (file->npp_instantiated)
  {
    PLUGIN_CALL(file, destroy, file->instance, NULL);
    file->npp_instantiated = FALSE;
  }
  return TRUE;
}

gboolean
flash_file_resize (FlashFile *file, gint width, gint height, GError **error)
{
  /* FIXME */
  return TRUE;
}

void
flash_file_set_notify (FlashFile *file, const gchar *notify_url, void *notify_data)
{

  if (file->notify_url)
    g_free (file->notify_url);
  file->notify_url = g_strdup (notify_url);
  file->notify_data = notify_data;
}

static void
flash_file_class_init (FlashFileClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS (klass);
  object_class->finalize = flash_file_finalize;
}

static void
flash_file_reset (FlashFile *file)
{
  file->path = NULL;
  file->library = NULL;
  file->instance = NULL;
  file->npp_instantiated = FALSE;
  file->xt_bin = NULL;

  file->notify_url = NULL;
  file->notify_data = NULL;

  file->is_playing = FALSE;

  file->callback = NULL;
  file->callback_data = NULL;
}

static void
flash_file_init (FlashFile *file)
{
  flash_file_reset(file);
}

static void
flash_file_finalize (GObject *object)
{
  FlashFile *file;

  file = FLASH_FILE (object);
  if (file->notify_url)
    g_free (file->notify_url);

  flash_file_stop (file);

  if (file->instance)
  {
    file->instance->ndata = NULL;
    g_free (file->instance);
  }

  if (file->library)
    g_object_unref (file->library);

  if (file->path)
    g_free (file->path);

  flash_file_reset(file);
}

static gboolean
flash_file_new_attrs(int *argcp, char ***argnp, char ***argvp, ...)
{
  va_list ap;
  int va_argc;
  int argc;
  char **argn;
  char **argv;
  int i;
  int j;

  va_argc = 0;
  va_start (ap, argvp);
  while (va_arg (ap, char *) != NULL)
    va_argc++;
  va_end (ap);

  if (va_argc % 2 != 0)
    return FALSE;

  argc = va_argc / 2;
  argn = g_malloc (sizeof(char *) * argc);
  argv = g_malloc (sizeof(char *) * argc);

  va_start (ap, argvp);
  i = 1;
  j = 0;
  while (j < argc)
  {
    if (i % 2 != 0)
    {
      argn[j] = g_strdup (va_arg (ap, char *));
      argv[j] = g_strdup (va_arg (ap, char *));
      j++;
    }
    i++;
  }
  va_end (ap);

  *argcp = argc;
  *argnp = argn;
  *argvp = argv;
  return TRUE;
}

static void
flash_file_free_attrs (int argc, char **argn, char **argv)
{
  int i;
  for (i = 0; i < argc; i++)
  {
    g_free (argv[i]);
    g_free (argn[i]);
  }
  g_free (argv);
  g_free (argn);
}

static gchar *
flash_file_make_file_url (const gchar *path)
{
#define MAX_URL 2048
  char buf[MAX_URL+1];

  memset (buf, 0, sizeof(buf));
  snprintf (buf, sizeof(buf), "file:%s", path);

  return g_strdup (buf);
}

static gboolean
flash_file_stream_buf_to_plugin (FlashFile *file, const gchar *url,
                                 const gchar *mime_type, uint16 stype,
                                 void *buf, int buf_size, void *notify_data,
                                 GError **error)
{
  NPStream *npstream;
  NPError nperr;
  int buf_remaining;
  off_t buf_offset;
  char *cbuf;
  int32 plugin_maxwrite;
  int32 plugin_nwrite;
  int32 plugin_nwritten;

  npstream = g_new (NPStream, 1);
  npstream->ndata = file;
  npstream->url = g_strdup (url);
  npstream->end = 0;
  npstream->pdata = NULL;
  npstream->lastmodified = 0;
  npstream->notifyData = notify_data;

  nperr = PLUGIN_CALL (file, newstream,
    file->instance,
    (char *)mime_type,
    npstream,
    0, /* default to non-seekable for all */
    &stype);

  if (nperr != NPERR_NO_ERROR)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_PLAY, "%s",
                 "Failed to create new stream");
    g_free ((gchar *)npstream->url);
    g_free (npstream);
    return FALSE;
  }

  buf_remaining = buf_size;
  buf_offset = 0;
  cbuf = (char *)buf;
  while (buf_remaining > 0)
  {
    plugin_maxwrite = PLUGIN_CALL (file, writeready, file->instance, npstream);
    plugin_nwrite = plugin_maxwrite > buf_remaining ? buf_remaining : plugin_maxwrite;
    plugin_nwritten = PLUGIN_CALL (file, write, file->instance, npstream, buf_offset,
                                   plugin_nwrite, (void *)cbuf);

    DEBUG ("%s: %d bytes streamed to plugin", url, plugin_nwritten);

    buf_remaining -= plugin_nwritten;
    buf_offset += plugin_nwritten;
    cbuf += plugin_nwritten;
  }

  if (notify_data)
    PLUGIN_CALL (file, urlnotify, file->instance, npstream->url, NPRES_DONE,
                 notify_data);

  PLUGIN_CALL (file, destroystream, file->instance, npstream, NPRES_DONE);
  g_free ((gchar *)npstream->url);
  g_free (npstream);

  return TRUE;
}

static gboolean
flash_file_send_to_plugin (FlashFile *file, const gchar *url, GError **error)
{
  int map_fd;
  void *map;
  struct stat sb;
  gchar *js_buf;
  gboolean ret;

  map_fd = -1;
  map = NULL;
  js_buf = NULL;
  ret = FALSE;

  if (stat (file->path, &sb) == -1)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS,
                 "Failed to stat '%s': %s", file->path, strerror(errno));
    goto out;
  }

  map_fd = open (file->path, O_RDONLY);
  if (map_fd == -1)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS,
                 "Failed to open() '%s': %s", file->path, strerror(errno));
    goto out;
  }
  map = mmap (0, sb.st_size, PROT_READ, MAP_SHARED, map_fd, 0);
  if (map == MAP_FAILED)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS,
                 "Failed to mmap() '%s': %s", file->path, strerror(errno));
    goto out;
  }

  if (!flash_file_stream_buf_to_plugin (file, url, MIME_TYPE, NP_ASFILE,
                                        map, sb.st_size, NULL, error))
  {
    goto out;
  }

  if (!file->notify_url)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_PLAY, "%s",
                 "Failed to receive ancillary notification request");
    goto out;
  }

  js_buf = g_strdup ("null");
  if (!flash_file_stream_buf_to_plugin (file, file->notify_url, "text/plain",
                                        NP_NORMAL, js_buf, strlen (js_buf)+1,
                                        file->notify_data, error))
  {
    goto out;
  }
  ret = TRUE;

out:
  if (js_buf)
    g_free (js_buf);
  if (map)
    munmap (map, sb.st_size);
  if (map_fd != -1)
    close (map_fd);
  return ret;
}

static void *
flash_file_get_script_peer (FlashFile *file)
{
  NPError nperr;
  void *peer_instance;

  peer_instance = NULL;
  nperr = PLUGIN_CALL (file, getvalue, file->instance, NPPVpluginScriptableInstance, &peer_instance);
  if (nperr == NPERR_NO_ERROR && peer_instance != NULL)
    return peer_instance;
  return NULL;
}

static void
flash_file_release_script_peer (FlashFile *file, void *peer)
{
  if (file->library->spf_release)
    file->library->spf_release (peer);
}

static gboolean
flash_file_timer_callback (gpointer data)
{
  FlashFile *file;

  file = (FlashFile *) data;
  if (!flash_file_is_playing (file))
  {
    if (file->callback)
      file->callback (file, FLASH_FILE_PLAYBACK_STOPPED, file->callback_data);
    return FALSE;
  }
  return TRUE;
}
