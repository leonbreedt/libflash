/* Flash Plugin Wrapper Library
 * (C) Copyright 2004-2005 Leon Breedt
 *
 * Licensed under the terms of the MIT license.
 */

#include <glib.h>
#include <glib-object.h>
#include <gtk/gtk.h>
#include <flash/flash.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

static FlashLibrary *library = NULL;
static FlashFile *file = NULL;

static void
on_flash_event (FlashFile *file, FlashFileEvent event, gpointer data)
{
	switch (event) {
		case FLASH_FILE_PLAYBACK_STOPPED:
			fprintf (stderr, "Playback of Flash file %p has stopped\n", file);
			gtk_main_quit ();
			break;
		default:
			fprintf (stderr, "Ignoring unhandled Flash event %d", event);
	}
}

int
main(int argc, char **argv)
{
  const char *swf_path;
  gchar *plugin_path;
  const gchar *description;
  GError *error;
  GtkWidget *window;

  if (argc < 2)
  {
    fprintf (stderr, "usage: %s file.swf [plugin.so]\n", argv[0]);
    return 1;
  }
  swf_path = argv[1];
  if (argc > 2)
    plugin_path = g_strdup (argv[2]);
  else
    plugin_path = g_strdup ("./libflashplayer.so");

  fprintf(stderr, "Playing SWF file '%s'\n", swf_path);

  if (!flash_init (&argc, &argv))
  {
    fprintf (stderr, "error: failed to initialize Flash library\n");
    return 1;
  }

  error = NULL;
  library = flash_library_new (plugin_path, &error);
  if (error != NULL)
  {
    fprintf (stderr, "error: failed to load Flash library: %s\n", error->message);
    g_error_free (error);
    g_free (plugin_path);
    return 1;
  }
  g_free (plugin_path);

  g_object_get (library, "description", &description, NULL);
  printf("testflash (%s)\n", description);

  error = NULL;
  file = flash_file_new (library, swf_path, on_flash_event, NULL, &error);
  if (error != NULL)
  {
    g_object_unref (library);
    fprintf(stderr, "error: failed to load Flash file: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
  gtk_window_set_title (GTK_WINDOW (window), "Flash");
  gtk_widget_show (window);

  error = NULL;
  flash_file_play (file, GTK_WINDOW(window), FALSE, &error);
  if (error != NULL)
  {
    g_object_unref (file);
    g_object_unref (library);
    fprintf(stderr, "error: failed to play Flash file: %s\n", error->message);
    g_error_free (error);
    return 1;
  }

  printf ("File '%s' started (playing=%d)\n", swf_path, flash_file_is_playing (file));

  g_signal_connect (window, "delete-event", (void (*)(void))gtk_main_quit, NULL);
  signal (2, (void (*)(int))gtk_main_quit);
  gtk_main ();

#if 0
  flash_file_pause (file);
  gtk_main ();
  flash_file_resume (file);
  gtk_main ();
#endif

  g_object_unref (file);
  g_object_unref (library);

  printf ("File '%s' stopped\n", swf_path);
  return 0;
}
