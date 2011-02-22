/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#ifndef __FLASH_COMMON_H__
#define __FLASH_COMMON_H__

#include <glib.h>

G_BEGIN_DECLS

enum
{
  FLASH_ERROR = 1
};

enum
{
  FLASH_ERROR_INIT_FAILED    = 1002,

  FLASH_ERROR_FILE_ACCESS           = 3000,
  FLASH_ERROR_FILE_PLAY             = 3001,
};

#define FLASH_DEBUG 1
#if FLASH_DEBUG
#define DEBUG(fmt, x...) g_log ("Flash", G_LOG_LEVEL_DEBUG, fmt, ##x)
#else
#define DEBUG(fmt, x...)
#endif

gboolean flash_init(int *argc, char ***argv);

gchar   *flash_canonicalize_path (const gchar *path);
gboolean flash_is_valid_file     (const gchar *path, const gchar **allowed_exts,
                                  GError **error);


G_END_DECLS

#endif
