/* text-viewer-window.c
 *
 * Copyright 2025 memorycancel
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include "text-viewer-window.h"

g_autoptr (GFile) opened_file = NULL;

struct _TextViewerWindow
{
	AdwApplicationWindow  parent_instance;

  GSettings *settings;

	/* Template widgets */
  AdwHeaderBar *header_bar;
  GtkTextView *main_text_view;
  GtkButton *open_button;
  GtkLabel *cursor_pos;
  AdwToastOverlay *toast_overlay;
};

G_DEFINE_FINAL_TYPE (TextViewerWindow, text_viewer_window, ADW_TYPE_APPLICATION_WINDOW)

static void
text_viewer_window_finalize (GObject *gobject)
{
  TextViewerWindow *self = TEXT_VIEWER_WINDOW (gobject);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (text_viewer_window_parent_class)->finalize (gobject);
}

static void
text_viewer_window_class_init (TextViewerWindowClass *klass)
{
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = text_viewer_window_finalize;

	gtk_widget_class_set_template_from_resource (widget_class, "/com/TommyTech/TextViewer/text-viewer-window.ui");
  gtk_widget_class_bind_template_child (widget_class, TextViewerWindow, main_text_view);
  gtk_widget_class_bind_template_child (widget_class, TextViewerWindow, open_button);
  gtk_widget_class_bind_template_child (widget_class, TextViewerWindow, cursor_pos);
  gtk_widget_class_bind_template_child (widget_class, TextViewerWindow, toast_overlay);
}

static void
open_file_complete (GObject          *source_object,
                    GAsyncResult     *result,
                    TextViewerWindow *self)
{
  GFile *file = G_FILE (source_object);

  g_autofree char *contents = NULL;
  gsize length = 0;

  g_autoptr (GError) error = NULL;

  // Complete the asynchronous operation; this function will either
  // give you the contents of the file as a byte array, or will
  // set the error argument
  g_file_load_contents_finish (file,
                               result,
                               &contents,
                               &length,
                               NULL,
                               &error);
    // Query the display name for the file
  g_autofree char *display_name = NULL;
  g_autoptr (GFileInfo) info =
    g_file_query_info (file,
                       "standard::display-name",
                       G_FILE_QUERY_INFO_NONE,
                       NULL,
                       NULL);
  if (info != NULL)
    {
      display_name =
        g_strdup (g_file_info_get_attribute_string (info, "standard::display-name"));
    }
  else
    {
      display_name = g_file_get_basename (file);
    }

    // In case of error, show a toast
  if (error != NULL)
    {
      g_autofree char *msg =
        g_strdup_printf ("Unable to open “%s”", display_name);

      adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (msg));
      return;
    }

  // Ensure that the file is encoded with UTF-8
  if (!g_utf8_validate (contents, length, NULL))
    {
      g_autofree char *msg =
        g_strdup_printf ("Invalid text encoding for “%s”", display_name);

      adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (msg));
      return;
    }

  // In case of error, print a warning to the standard error output
  if (error != NULL)
    {
      g_printerr ("Unable to open “%s”: %s\n",
                  g_file_peek_path (file),
                  error->message);
      return;
    }

  // Ensure that the file is encoded with UTF-8
  if (!g_utf8_validate (contents, length, NULL))
    {
      g_printerr ("Unable to load the contents of “%s”: "
                  "the file is not encoded with UTF-8\n",
                  g_file_peek_path (file));
      return;
    }

  // Retrieve the GtkTextBuffer instance that stores the
  // text displayed by the GtkTextView widget
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);

  // Set the text using the contents of the file
  gtk_text_buffer_set_text (buffer, contents, length);

  // Reposition the cursor so it's at the start of the text
  GtkTextIter start;
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_place_cursor (buffer, &start);

  // Set the title using the display name
  gtk_window_set_title (GTK_WINDOW (self), display_name);

  // Show a toast for the successful loading
  g_autofree char *msg =
    g_strdup_printf ("Opened “%s”", display_name);

  adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (msg));
}

static void
open_file (TextViewerWindow *self,
           GFile            *file)
{
  g_file_load_contents_async (file,
                              NULL,
                              (GAsyncReadyCallback) open_file_complete,
                              self);
}

static void
on_open_response (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  TextViewerWindow *self = user_data;

  opened_file =
    gtk_file_dialog_open_finish (dialog, result, NULL);

  // Ubuntu 桌面文件的副本之所以会出现在 /run/user/1000/doc 目录中，
  // 是因为 Flatpak 应用程序访问系统文件的方式；它们基本上是为系统创建了一个临时的、
  // 隔离的 "沙盒 "视图，包括在这个位置上的桌面文件副本，
  // 允许它们访问和修改这些文件，而不直接与实际文件系统交互。
  // g_autofree char *path = g_file_get_path(file); // Get the real path
  // g_print("Real path: %s\n", path); // Print the real path

  // If the user selected a file, open it
  if (opened_file != NULL)
    open_file (self, opened_file);
}

static void
text_viewer_window__open_file_dialog (GAction          *action G_GNUC_UNUSED,
                                      GVariant         *parameter G_GNUC_UNUSED,
                                      TextViewerWindow *self)
{
  g_autoptr (GtkFileDialog) dialog = gtk_file_dialog_new ();

  gtk_file_dialog_open (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        on_open_response,
                        self);
}

static void
text_viewer_window__update_cursor_position (GtkTextBuffer    *buffer,
                                            GParamSpec       *pspec G_GNUC_UNUSED,
                                            TextViewerWindow *self)
{
  int cursor_pos = 0;

  // Retrieve the value of the "cursor-position" property
  g_object_get (buffer, "cursor-position", &cursor_pos, NULL);

  // Construct the text iterator for the position of the cursor
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, cursor_pos);

  // Set the new contents of the label
  g_autofree char *cursor_str =
    g_strdup_printf ("Ln %d, Col %d",
                     gtk_text_iter_get_line (&iter) + 1,
                     gtk_text_iter_get_line_offset (&iter) + 1);

  gtk_label_set_text (self->cursor_pos, cursor_str);
}

static void
save_file_complete (GObject      *source_object,
                    GAsyncResult *result,
                    gpointer      user_data)
{
  GFile *file = G_FILE (source_object);
  TextViewerWindow *self = user_data;

  g_autoptr (GError) error =  NULL;
  g_file_replace_contents_finish (file, result, NULL, &error);

  // Query the display name for the file
  g_autofree char *display_name = NULL;
  g_autoptr (GFileInfo) info =
  g_file_query_info (file,
                     "standard::display-name",
                     G_FILE_QUERY_INFO_NONE,
                     NULL,
                     NULL);
  if (info != NULL)
    {
      display_name =
        g_strdup (g_file_info_get_attribute_string (info, "standard::display-name"));
    }
  else
    {
      display_name = g_file_get_basename (file);
    }

  if (error != NULL)
    {
      g_printerr ("Unable to save “%s”: %s\n",
                  display_name,
                  error->message);
    }

  g_autofree char *msg = NULL;
  if (error != NULL)
    msg = g_strdup_printf ("Unable to save as “%s”", display_name);
  else
    msg = g_strdup_printf ("Saved as “%s”", display_name);

  adw_toast_overlay_add_toast (self->toast_overlay, adw_toast_new (msg));
}

static void
 save_file (TextViewerWindow *self,
            GFile            *file)
 {
   GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);

   // Retrieve the iterator at the start of the buffer
   GtkTextIter start;
   gtk_text_buffer_get_start_iter (buffer, &start);

   // Retrieve the iterator at the end of the buffer
   GtkTextIter end;
   gtk_text_buffer_get_end_iter (buffer, &end);

   // Retrieve all the visible text between the two bounds
   char *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

   // If there is nothing to save, return early
   if (text == NULL)
     return;

   g_autoptr(GBytes) bytes = g_bytes_new_take (text, strlen (text));

   // Start the asynchronous operation to save the data into the file
   g_file_replace_contents_bytes_async (file,
                                        bytes,
                                        NULL,
                                        FALSE,
                                        G_FILE_CREATE_NONE,
                                        NULL,
                                        save_file_complete,
                                        self);
}

static void
on_save_response (GObject      *source,
                  GAsyncResult *result,
                  gpointer      user_data)
{
  GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
  TextViewerWindow *self = user_data;

  opened_file = gtk_file_dialog_save_finish (dialog, result, NULL);

  if (opened_file != NULL)
    save_file (self, opened_file);
}

static void
text_viewer_window__save_file_dialog (GAction          *action G_GNUC_UNUSED,
                                      GVariant         *param G_GNUC_UNUSED,
                                      TextViewerWindow *self)
{
  g_autoptr (GtkFileDialog) dialog =
    gtk_file_dialog_new ();

  gtk_file_dialog_save (dialog,
                        GTK_WINDOW (self),
                        NULL,
                        on_save_response,
                        self);
}

static void
just_save_the_opened_file (GAction          *action,
                           GVariant         *param,
                           TextViewerWindow *self)
{
  if (opened_file == NULL) {
    g_printerr ("No file is currently opened to save.\n");
    return;
  }

  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);

  // Retrieve the iterator at the start of the buffer
  GtkTextIter start;
  gtk_text_buffer_get_start_iter (buffer, &start);

  // Retrieve the iterator at the end of the buffer
  GtkTextIter end;
  gtk_text_buffer_get_end_iter (buffer, &end);

  // Retrieve all the visible text between the two bounds
  char *text = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

  // If there is nothing to save, return early
  if (text == NULL)
    return;

  g_autoptr(GBytes) bytes = g_bytes_new_take (text, strlen (text));

  // Start the asynchronous operation to save the data into the file
  g_file_replace_contents_bytes_async (opened_file,
                                       bytes,
                                       NULL,
                                       FALSE,
                                       G_FILE_CREATE_NONE,
                                       NULL,
                                       save_file_complete,
                                       self);
}

static void
text_viewer_window_init (TextViewerWindow *self)
{
	gtk_widget_init_template (GTK_WIDGET (self));

  g_autoptr (GSimpleAction) open_action =
    g_simple_action_new ("open", NULL);
  g_signal_connect (open_action,
                    "activate",
                    G_CALLBACK (text_viewer_window__open_file_dialog),
                    self);
  g_action_map_add_action (G_ACTION_MAP (self),
                           G_ACTION (open_action));

  g_autoptr (GSimpleAction) save_action = g_simple_action_new ("save-as", NULL);
  g_signal_connect (save_action, "activate", G_CALLBACK (text_viewer_window__save_file_dialog), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (save_action));

  g_autoptr (GSimpleAction) just_save_action = g_simple_action_new ("just-save", NULL);
  g_signal_connect (just_save_action, "activate", G_CALLBACK (just_save_the_opened_file), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (just_save_action));

  GtkTextBuffer *buffer = gtk_text_view_get_buffer (self->main_text_view);
  g_signal_connect (buffer,
                    "notify::cursor-position",
                    G_CALLBACK (text_viewer_window__update_cursor_position),
                    self);

  self->settings = g_settings_new ("com.TommyTech.TextViewer");

  g_settings_bind (self->settings, "window-width",
                   G_OBJECT (self), "default-width",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "window-height",
                   G_OBJECT (self), "default-height",
                   G_SETTINGS_BIND_DEFAULT);
  g_settings_bind (self->settings, "window-maximized",
                   G_OBJECT (self), "maximized",
                   G_SETTINGS_BIND_DEFAULT);
}
