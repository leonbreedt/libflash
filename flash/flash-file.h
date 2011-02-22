/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#ifndef __FLASH_FILE_H__
#define __FLASH_FILE_H__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <flash/flash-library.h>

G_BEGIN_DECLS

typedef struct _FlashFile      FlashFile;
typedef struct _FlashFileClass FlashFileClass;

typedef enum {
  FLASH_FILE_PLAYBACK_STOPPED
} FlashFileEvent;

typedef void (*FlashFileEventCallback)(FlashFile *file, FlashFileEvent event,
                                       gpointer user_data);

#define FLASH_TYPE_FILE \
  (flash_file_get_type())

#define FLASH_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), FLASH_TYPE_FILE, FlashFile))

#define FLASH_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), FLASH_TYPE_FILE, FlashFileClass))

#define FLASH_IS_FILE(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLASH_TYPE_FILE))

#define FLASH_IS_FILE_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), FLASH_TYPE_FILE))

#define FLASH_FILE_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), FLASH_TYPE_FILE, FlashFileClass))

GType flash_file_get_type (void);

FlashFile *flash_file_new        (FlashLibrary *library, const gchar *path,
                                  FlashFileEventCallback callback,
                                  gpointer callback_user_data,
                                  GError **error);
gboolean   flash_file_play       (FlashFile *file, GtkWindow *window,
                                  gboolean loop, GError **error);
gboolean   flash_file_is_playing (FlashFile *file);
gboolean   flash_file_pause      (FlashFile *file);
gboolean   flash_file_resume     (FlashFile *file);
gboolean   flash_file_stop       (FlashFile *file);

gboolean   flash_file_resize     (FlashFile *file, gint width, gint height, GError **error);
 
G_END_DECLS

#endif
