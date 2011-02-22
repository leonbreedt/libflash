/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#ifndef __FLASH_LIBRARY_H__
#define __FLASH_LIBRARY_H__

#include <glib-object.h>
#include <gmodule.h>

G_BEGIN_DECLS

typedef struct _FlashLibrary      FlashLibrary;
typedef struct _FlashLibraryClass FlashLibraryClass;

#define FLASH_TYPE_LIBRARY \
  (flash_library_get_type())

#define FLASH_LIBRARY(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj), FLASH_TYPE_LIBRARY, FlashLibrary))

#define FLASH_LIBRARY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass), FLASH_TYPE_LIBRARY, FlashLibraryClass))

#define FLASH_IS_LIBRARY(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj), FLASH_TYPE_LIBRARY))

#define FLASH_IS_LIBRARY_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass), FLASH_TYPE_LIBRARY))

#define FLASH_LIBRARY_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS((obj), FLASH_TYPE_LIBRARY, FlashLibraryClass))

GType flash_library_get_type (void);

FlashLibrary *flash_library_new (const gchar *path, GError **error);

G_END_DECLS

#endif
