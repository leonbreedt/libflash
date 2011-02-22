/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#ifndef __FLASH_FILE_INTERNAL_H__
#define __FLASH_FILE_INTERNAL_H__

#include <glib.h>
#include "flash-file.h"
#include "npupp.h"

G_BEGIN_DECLS

void flash_file_set_notify (FlashFile *file, const gchar *notify_url, void *notify_data);

G_END_DECLS

#endif
