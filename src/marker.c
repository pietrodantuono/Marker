/* marker.c
 *
 * Copyright (C) 2017-2026 Marker contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker.h"

#include <adwaita.h>
#include <glib/gi18n.h>
#include <gtksourceview/gtksource.h>
#include <libspelling.h>
#include <stdlib.h>

#include "marker-exporter.h"
#include "marker-prefs.h"
#include "marker-session.h"
#include "marker-window.h"

static GtkApplication *app;
static gboolean initialized;
static gboolean quitting;
static gboolean editor_mode_arg;
static gboolean preview_mode_arg;
static gboolean dual_pane_mode_arg;
static gboolean dual_window_mode_arg;
static gboolean formatted_mode_arg;
static gchar *outfile_arg;
static gint command_status = -1;

static const GOptionEntry cli_options[] = {
  { "editor", 'e', 0, G_OPTION_ARG_NONE, &editor_mode_arg, "Open in editor-only mode", NULL },
  { "preview", 'p', 0, G_OPTION_ARG_NONE, &preview_mode_arg, "Open in preview-only mode", NULL },
  { "dual-pane", 'd', 0, G_OPTION_ARG_NONE, &dual_pane_mode_arg, "Open in dual-pane mode", NULL },
  { "dual-window", 'w', 0, G_OPTION_ARG_NONE, &dual_window_mode_arg, "Open in dual-window mode", NULL },
  { "formatted", 'f', 0, G_OPTION_ARG_NONE, &formatted_mode_arg, "Open in Formatted Markdown mode", NULL },
  { "output", 'o', 0, G_OPTION_ARG_STRING, &outfile_arg, "Export Markdown to the given output file", "FILE" },
  { NULL }
};

GtkApplication *
marker_get_app (void)
{
  return app;
}

static void
load_application_css (void)
{
  GtkCssProvider *provider = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (provider,
    "/com/github/fabiocolacio/marker/styles/marker.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (),
                                              GTK_STYLE_PROVIDER (provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref (provider);
}

static void
marker_init (void)
{
  if (initialized)
    return;
  initialized = TRUE;

  marker_prefs_load ();
  load_application_css ();
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_for_display (gdk_display_get_default ()),
                                    "/com/github/fabiocolacio/marker/icons");
  g_set_application_name ("Marker");
  g_set_prgname ("com.github.fabiocolacio.marker");

  const struct { const char *action; const char *accels[3]; } shortcuts[] = {
    { "app.new", { "<Ctrl><Shift>N", NULL } },
    { "app.quit", { "<Ctrl>Q", NULL } },
    { "win.new-document", { "<Ctrl>N", NULL } },
    { "win.open-file", { "<Ctrl>O", NULL } },
    { "win.open-folder", { "<Ctrl><Shift>O", NULL } },
    { "win.save", { "<Ctrl>S", NULL } },
    { "win.save-as", { "<Ctrl><Shift>S", NULL } },
    { "win.close-document", { "<Ctrl>W", NULL } },
    { "win.search", { "<Ctrl>F", NULL } },
    { "win.bold", { "<Ctrl>B", NULL } },
    { "win.italic", { "<Ctrl>I", NULL } },
    { "win.monospace", { "<Ctrl>M", NULL } },
    { "win.link", { "<Ctrl>K", NULL } },
    { "win.view-mode::editor", { "<Ctrl>1", NULL } },
    { "win.view-mode::preview", { "<Ctrl>2", NULL } },
    { "win.view-mode::dual-pane", { "<Ctrl>3", NULL } },
    { "win.view-mode::dual-window", { "<Ctrl>4", NULL } },
    { "win.view-mode::formatted", { "<Ctrl>5", NULL } },
    { "win.focus", { "F11", NULL } },
    { "win.escape", { "Escape", NULL } },
  };

  for (guint i = 0; i < G_N_ELEMENTS (shortcuts); i++)
    gtk_application_set_accels_for_action (app, shortcuts[i].action,
                                           shortcuts[i].accels);
}

static void
apply_cli_view_mode (MarkerWindow *window)
{
  MarkerEditor *editor = marker_window_get_active_editor (window);
  if (editor == NULL)
    return;
  if (formatted_mode_arg)
    marker_editor_set_view_mode (editor, FORMATTED_MODE);
  else if (preview_mode_arg)
    marker_editor_set_view_mode (editor, PREVIEW_ONLY_MODE);
  else if (editor_mode_arg)
    marker_editor_set_view_mode (editor, EDITOR_ONLY_MODE);
  else if (dual_pane_mode_arg)
    marker_editor_set_view_mode (editor, DUAL_PANE_MODE);
  else if (dual_window_mode_arg)
    marker_editor_set_view_mode (editor, DUAL_WINDOW_MODE);
}

void
marker_create_new_window (void)
{
  MarkerWindow *window = marker_window_new (app);
  marker_window_new_editor (window);
  gtk_window_present (GTK_WINDOW (window));
  apply_cli_view_mode (window);
}

void
marker_create_new_window_from_file (GFile *file)
{
  MarkerWindow *window = marker_window_new_from_file (app, file);
  gtk_window_present (GTK_WINDOW (window));
  apply_cli_view_mode (window);
}

void
marker_create_new_window_from_workspace (GFile *folder)
{
  g_autoptr (GError) error = NULL;
  MarkerWindow *window = marker_window_new_from_workspace (app, folder, &error);
  if (window == NULL)
    {
      g_warning ("Unable to open project: %s", error->message);
      return;
    }
  gtk_window_present (GTK_WINDOW (window));
  apply_cli_view_mode (window);
}

void
marker_open_file (GFile *file)
{
  GtkWindow *active = gtk_application_get_active_window (app);
  if (MARKER_IS_WINDOW (active))
    marker_window_new_editor_from_file (MARKER_WINDOW (active), file);
  else
    marker_create_new_window_from_file (file);
}

static void
restore_session (void)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GPtrArray) windows = marker_session_load (&error);

  if (windows == NULL || windows->len == 0)
    {
      marker_create_new_window ();
      return;
    }

  for (guint i = 0; i < windows->len; i++)
    {
      MarkerSessionWindow *saved = g_ptr_array_index (windows, i);
      MarkerWindow *window = marker_window_new (app);

      for (guint j = 0; j < saved->projects->len; j++)
        {
          g_autoptr (GFile) folder = g_file_new_for_uri (g_ptr_array_index (saved->projects, j));
          marker_window_add_project (window, folder);
        }
      for (guint j = 0; j < saved->documents->len; j++)
        {
          g_autoptr (GFile) file = g_file_new_for_uri (g_ptr_array_index (saved->documents, j));
          marker_window_restore_saved_document (window, file);
        }
      if (saved->documents->len == 0)
        marker_window_new_editor (window);
      marker_window_set_active_project_index (window, saved->active_project);
      marker_window_set_active_document_uri (window, saved->active_document);
      marker_window_restore_navigation (window, saved);
      gtk_window_present (GTK_WINDOW (window));
    }
}

static void
activate_cb (GtkApplication *application,
             gpointer        user_data)
{
  marker_init ();
  if (editor_mode_arg || preview_mode_arg || dual_pane_mode_arg ||
      dual_window_mode_arg || formatted_mode_arg)
    marker_create_new_window ();
  else
    restore_session ();
}

static void
open_cb (GtkApplication *application,
         GFile         **files,
         gint            n_files,
         const char     *hint,
         gpointer        user_data)
{
  MarkerWindow *window;

  marker_init ();
  if (outfile_arg != NULL && n_files > 0)
    {
      g_autofree char *input = g_file_get_path (files[0]);
      g_autoptr (GFile) output_file = g_file_new_for_commandline_arg (outfile_arg);
      g_autofree char *output = g_file_get_path (output_file);
      command_status = marker_exporter_export (input, output)
                         ? EXIT_SUCCESS : EXIT_FAILURE;
      g_application_quit (G_APPLICATION (application));
      return;
    }

  window = marker_window_new (application);
  for (gint i = 0; i < n_files; i++)
    {
      GFileType type = g_file_query_file_type (files[i], G_FILE_QUERY_INFO_NONE, NULL);
      if (type == G_FILE_TYPE_DIRECTORY)
        marker_window_add_project (window, files[i]);
      else
        marker_window_new_editor_from_file (window, files[i]);
    }
  if (marker_window_get_active_editor (window) == NULL)
    marker_window_new_editor (window);
  apply_cli_view_mode (window);
  gtk_window_present (GTK_WINDOW (window));
}

void
marker_save_session (void)
{
  g_autoptr (GPtrArray) saved = NULL;

  if (app == NULL)
    return;
  saved = g_ptr_array_new_with_free_func ((GDestroyNotify) marker_session_window_free);
  for (GList *item = gtk_application_get_windows (app); item != NULL; item = item->next)
    if (MARKER_IS_WINDOW (item->data))
      g_ptr_array_add (saved, marker_window_capture_session (MARKER_WINDOW (item->data)));
  if (saved->len > 0)
    marker_session_save (saved, NULL);
}

static void
save_session_cb (GApplication *application,
                 gpointer      user_data)
{
  marker_save_session ();
}

static void
window_removed_cb (GtkApplication *application,
                   GtkWindow      *window,
                   gpointer        user_data)
{
  if (!quitting && gtk_application_get_windows (application) != NULL)
    marker_save_session ();
}

void
new_cb (GSimpleAction *action,
        GVariant      *parameter,
        gpointer       user_data)
{
  marker_create_new_window ();
}

void
marker_prefs_cb (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  marker_prefs_show_window ();
}

void
marker_help_cb (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  GtkUriLauncher *launcher = gtk_uri_launcher_new ("help:Marker");
  gtk_uri_launcher_launch (launcher, gtk_application_get_active_window (app),
                           NULL, NULL, NULL);
  g_object_unref (launcher);
}

void
marker_about_cb (GSimpleAction *action,
                 GVariant      *parameter,
                 gpointer       user_data)
{
  const char *authors[] = { "Fabio Colacio", "Martino Ferrari", "Marker contributors", NULL };
  GtkWidget *dialog = gtk_about_dialog_new ();
  gtk_about_dialog_set_logo_icon_name (GTK_ABOUT_DIALOG (dialog),
                                       "com.github.fabiocolacio.marker");
  gtk_about_dialog_set_program_name (GTK_ABOUT_DIALOG (dialog), "Marker");
  gtk_about_dialog_set_version (GTK_ABOUT_DIALOG (dialog), MARKER_VERSION);
  gtk_about_dialog_set_comments (GTK_ABOUT_DIALOG (dialog),
                                 _("A project-based Markdown editor for GNOME"));
  gtk_about_dialog_set_website (GTK_ABOUT_DIALOG (dialog),
                                "https://github.com/fabiocolacio/Marker");
  gtk_about_dialog_set_license_type (GTK_ABOUT_DIALOG (dialog), GTK_LICENSE_GPL_3_0);
  gtk_about_dialog_set_authors (GTK_ABOUT_DIALOG (dialog), authors);
  gtk_window_set_transient_for (GTK_WINDOW (dialog),
                                gtk_application_get_active_window (app));
  gtk_window_present (GTK_WINDOW (dialog));
}

void
marker_shortcuts_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  AdwDialog *dialog = adw_dialog_new ();
  GtkWidget *page = adw_status_page_new ();
  adw_dialog_set_title (dialog, _("Keyboard Shortcuts"));
  adw_dialog_set_content_width (dialog, 520);
  adw_dialog_set_content_height (dialog, 520);
  adw_status_page_set_icon_name (ADW_STATUS_PAGE (page), "preferences-desktop-keyboard-shortcuts-symbolic");
  adw_status_page_set_title (ADW_STATUS_PAGE (page), _("Keyboard Shortcuts"));
  adw_status_page_set_description (ADW_STATUS_PAGE (page),
    _("Ctrl+1…5  View modes\nCtrl+O  Open file\nCtrl+Shift+O  Open folder\nCtrl+S  Save\nCtrl+F  Find\nF11  Focus mode\nEscape  Leave focus mode"));
  adw_dialog_set_child (dialog, page);
  adw_dialog_present (dialog, GTK_WIDGET (gtk_application_get_active_window (app)));
}

void
marker_quit (void)
{
  GList *windows = g_list_copy (gtk_application_get_windows (app));
  gboolean pending = FALSE;

  marker_save_session ();
  quitting = TRUE;
  for (GList *item = windows; item != NULL; item = item->next)
    if (MARKER_IS_WINDOW (item->data))
      {
        if (marker_window_try_close (item->data))
          gtk_window_destroy (GTK_WINDOW (item->data));
        else
          pending = TRUE;
      }
  if (pending)
    quitting = FALSE;
  g_list_free (windows);
}

void
marker_quit_cb (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  marker_quit ();
}


const int APP_MENU_ACTION_ENTRIES_LEN = 6;
const GActionEntry APP_MENU_ACTION_ENTRIES[] = {
  { "new", new_cb },
  { "prefs", marker_prefs_cb },
  { "shortcuts", marker_shortcuts_cb },
  { "help", marker_help_cb },
  { "about", marker_about_cb },
  { "quit", marker_quit_cb },
};

int
main (int argc,
      char **argv)
{
  int status;

  bindtextdomain ("marker", LOCALE_DIR);
  bind_textdomain_codeset ("marker", "UTF-8");
  textdomain ("marker");

  gtk_source_init ();
  spelling_init ();
  app = GTK_APPLICATION (adw_application_new ("com.github.fabiocolacio.marker",
                                               G_APPLICATION_HANDLES_OPEN));
  g_action_map_add_action_entries (G_ACTION_MAP (app), APP_MENU_ACTION_ENTRIES,
                                   APP_MENU_ACTION_ENTRIES_LEN, app);
  g_application_add_main_option_entries (G_APPLICATION (app), cli_options);
  g_signal_connect (app, "activate", G_CALLBACK (activate_cb), NULL);
  g_signal_connect (app, "open", G_CALLBACK (open_cb), NULL);
  g_signal_connect (app, "window-removed", G_CALLBACK (window_removed_cb), NULL);
  g_signal_connect (app, "shutdown", G_CALLBACK (save_session_cb), NULL);
  status = g_application_run (G_APPLICATION (app), argc, argv);
  if (command_status >= 0)
    status = command_status;
  g_clear_object (&app);
  gtk_source_finalize ();
  return status;
}
