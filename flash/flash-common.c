/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <glib-object.h>
#include <gtk/gtk.h>

#include "flash-common.h"

gboolean
flash_init (int *argc, char ***argv)
{
  g_type_init ();
  if (!gtk_init_check (argc, argv))
    return FALSE;
  if (!g_thread_supported ())
    g_thread_init (NULL);
  return TRUE;
}

gchar *
flash_canonicalize_path (const gchar *path)
{
  char *canon_path_glibc;
  gchar *canon_path;

  /* canonicalize_file_name is a safe GNU glibc extension, safer
   * than realpath(3) */
  canon_path_glibc = canonicalize_file_name (path) ;
  if (canon_path_glibc == NULL)
    return NULL;
  canon_path = g_strdup (canon_path_glibc);
  free (canon_path_glibc);
  return canon_path;
}

gboolean
flash_is_valid_file (const gchar *path, const gchar **allowed_exts,
                     GError **error)
{
  struct stat sb;
  gboolean ret;

  if (stat (path, &sb) == -1)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS,
                 "%s", strerror(errno));
    return FALSE;
  }
  
  if (!S_ISREG (sb.st_mode))
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS,
                 "Not a file: %s", path);
    return FALSE;
  }

  if (access (path, R_OK) == -1)
  {
    g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS,
                 "Not readable: %s", path);
    return FALSE;
  }

  ret = TRUE;
  if (allowed_exts != NULL)
  {
    int i;
    gchar *path_lower;

    ret = FALSE;
    path_lower = g_ascii_strdown (path, -1);
    for (i = 0; allowed_exts[i] != NULL; i++)
    {
      if (g_str_has_suffix (path_lower, allowed_exts[i]))
      {
        ret = TRUE;
        break;
      }
    }
    g_free (path_lower);
    if (!ret)
      g_set_error (error, FLASH_ERROR, FLASH_ERROR_FILE_ACCESS,
                   "Not a valid file: %s", path);
  }

  return ret;
}
