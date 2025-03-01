/* text-viewer-application.c
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
#include <glib/gi18n.h>

#include "text-viewer-application.h"
#include "text-viewer-window.h"

struct _TextViewerApplication
{
	AdwApplication parent_instance;

  GSettings *settings;
};

G_DEFINE_FINAL_TYPE (TextViewerApplication, text_viewer_application, ADW_TYPE_APPLICATION)

TextViewerApplication *
text_viewer_application_new (const char        *application_id,
                             GApplicationFlags  flags)
{
	g_return_val_if_fail (application_id != NULL, NULL);

	return g_object_new (TEXT_VIEWER_TYPE_APPLICATION,
	                     "application-id", application_id,
	                     "flags", flags,
	                     NULL);
}

static void
text_viewer_application_activate (GApplication *app)
{
	GtkWindow *window;

	g_assert (TEXT_VIEWER_IS_APPLICATION (app));

	window = gtk_application_get_active_window (GTK_APPLICATION (app));

	if (window == NULL)
		window = g_object_new (TEXT_VIEWER_TYPE_WINDOW,
		                       "application", app,
		                       NULL);

	gtk_window_present (window);
}

static void
text_viewer_application_dispose (GObject *gobject)
{
  TextViewerApplication *self = TEXT_VIEWER_APPLICATION (gobject);

  g_clear_object (&self->settings);

  G_OBJECT_CLASS (text_viewer_application_parent_class)->dispose (gobject);
}

static void
text_viewer_application_class_init (TextViewerApplicationClass *klass)
{
	GApplicationClass *app_class = G_APPLICATION_CLASS (klass);
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->dispose = text_viewer_application_dispose;

	app_class->activate = text_viewer_application_activate;
}

static void
text_viewer_application_about_action (GSimpleAction *action,
                                      GVariant      *parameter,
                                      gpointer       user_data)
{
	static const char *developers[] = {"memorycancel", NULL};
	TextViewerApplication *self = user_data;
	GtkWindow *window = NULL;

	g_assert (TEXT_VIEWER_IS_APPLICATION (self));

	window = gtk_application_get_active_window (GTK_APPLICATION (self));

	adw_show_about_dialog (GTK_WIDGET (window),
	                       "application-name", "text-viewer",
	                       "application-icon", "com.TommyTech.TextViewer",
	                       "developer-name", "memorycancel",
	                       "translator-credits", _("translator-credits"),
	                       "version", "0.1.0",
	                       "developers", developers,
	                       "copyright", "© 2025 memorycancel",
	                       NULL);
}

static void
text_viewer_application_quit_action (GSimpleAction *action,
                                     GVariant      *parameter,
                                     gpointer       user_data)
{
	TextViewerApplication *self = user_data;

	g_assert (TEXT_VIEWER_IS_APPLICATION (self));

	g_application_quit (G_APPLICATION (self));
}

static const GActionEntry app_actions[] = {
	{ "quit", text_viewer_application_quit_action },
	{ "about", text_viewer_application_about_action },
};

static void
change_color_scheme (GSimpleAction         *action,
                     GVariant              *new_state,
                     TextViewerApplication *self)
{
  gboolean dark_mode = g_variant_get_boolean (new_state);

  AdwStyleManager *style_manager = adw_style_manager_get_default ();

  if (dark_mode)
    adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_FORCE_DARK);
  else
    adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_DEFAULT);

  g_simple_action_set_state (action, new_state);

  g_settings_set_boolean (self->settings, "dark-mode", dark_mode);
}

static void
toggle_dark_mode (GSimpleAction *action,
                  GVariant      *parameter G_GNUC_UNUSED,
                  gpointer       user_data G_GNUC_UNUSED)
{
  GVariant *state = g_action_get_state (G_ACTION (action));
  gboolean old_state = g_variant_get_boolean (state);
  gboolean new_state = !old_state;

  g_action_change_state (G_ACTION (action), g_variant_new_boolean (new_state));

  g_variant_unref (state);
}

static void
text_viewer_application_init (TextViewerApplication *self)
{
  self->settings = g_settings_new ("com.TommyTech.TextViewer");

  gboolean dark_mode = g_settings_get_boolean (self->settings, "dark-mode");
  AdwStyleManager *style_manager = adw_style_manager_get_default ();
  if (dark_mode)
    adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_FORCE_DARK);
  else
    adw_style_manager_set_color_scheme (style_manager, ADW_COLOR_SCHEME_DEFAULT);

  g_autoptr (GSimpleAction) dark_action =
    g_simple_action_new_stateful ("dark-mode", NULL, g_variant_new_boolean (dark_mode));
  g_signal_connect (dark_action, "activate", G_CALLBACK (toggle_dark_mode), self);
  g_signal_connect (dark_action, "change-state", G_CALLBACK (change_color_scheme), self);
  g_action_map_add_action (G_ACTION_MAP (self), G_ACTION (dark_action));

	g_action_map_add_action_entries (G_ACTION_MAP (self),
	                                 app_actions,
	                                 G_N_ELEMENTS (app_actions),
	                                 self);
	gtk_application_set_accels_for_action (GTK_APPLICATION (self),
	                                       "app.quit",
	                                       (const char *[]) { "<primary>q", NULL });

  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.open",
                                         (const char *[]) {
                                           "<Ctrl>o",
                                           NULL,
                                         });
  gtk_application_set_accels_for_action (GTK_APPLICATION (self),
                                         "win.save-as",
                                         (const char *[]) {
                                           "<Ctrl><Shift>s",
                                           NULL,
                                         });
}
