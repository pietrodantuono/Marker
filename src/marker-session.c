/* marker-session.c
 *
 * Copyright (C) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-session.h"

#include <glib/gstdio.h>

static GKeyFile *metadata;

static char *
state_path (void)
{
  const char *override = g_getenv ("MARKER_STATE_DIR");
  const char *base = override != NULL ? override : g_get_user_state_dir ();
  g_autofree char *directory = g_build_filename (base, "marker", NULL);

  if (g_mkdir_with_parents (directory, 0700) != 0)
    return NULL;
  return g_build_filename (directory, "session.ini", NULL);
}

static GKeyFile *
load_key_file (void)
{
  g_autoptr (GKeyFile) file = g_key_file_new ();
  g_autofree char *path = state_path ();

  if (path != NULL)
    g_key_file_load_from_file (file, path, G_KEY_FILE_KEEP_COMMENTS, NULL);
  return g_steal_pointer (&file);
}

static GKeyFile *
get_metadata (void)
{
  if (metadata == NULL)
    metadata = load_key_file ();
  return metadata;
}

static char *
project_group (const char *uri)
{
  g_autofree char *digest = g_compute_checksum_for_string (G_CHECKSUM_SHA256, uri, -1);
  return g_strdup_printf ("Project %s", digest);
}

static gboolean
save_key_file (GKeyFile *file,
               GError  **error)
{
  g_autofree char *path = state_path ();
  g_autofree char *data = NULL;
  gsize length = 0;

  if (path == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Unable to create the Marker state directory");
      return FALSE;
    }

  data = g_key_file_to_data (file, &length, error);
  return data != NULL && g_file_set_contents (path, data, length, error);
}

MarkerSessionWindow *
marker_session_window_new (void)
{
  MarkerSessionWindow *window = g_new0 (MarkerSessionWindow, 1);
  window->projects = g_ptr_array_new_with_free_func (g_free);
  window->documents = g_ptr_array_new_with_free_func (g_free);
  window->active_project = G_MAXUINT;
  window->pages_visible = TRUE;
  return window;
}

void
marker_session_window_free (MarkerSessionWindow *window)
{
  if (window == NULL)
    return;
  g_ptr_array_unref (window->projects);
  g_ptr_array_unref (window->documents);
  g_free (window->active_document);
  g_free (window);
}

static void
append_string_list (GPtrArray  *array,
                    char      **values,
                    gsize       length)
{
  for (gsize i = 0; i < length; i++)
    if (values[i] != NULL && *values[i] != '\0')
      g_ptr_array_add (array, g_strdup (values[i]));
}

GPtrArray *
marker_session_load (GError **error)
{
  g_autoptr (GKeyFile) file = load_key_file ();
  g_autoptr (GPtrArray) windows = g_ptr_array_new_with_free_func ((GDestroyNotify) marker_session_window_free);
  gint count = g_key_file_get_integer (file, "Session", "window-count", NULL);

  g_clear_pointer (&metadata, g_key_file_unref);
  metadata = g_key_file_ref (file);

  for (gint i = 0; i < count; i++)
    {
      g_autofree char *group = g_strdup_printf ("Window %d", i);
      g_auto (GStrv) projects = NULL;
      g_auto (GStrv) documents = NULL;
      gsize project_count = 0;
      gsize document_count = 0;
      MarkerSessionWindow *window;

      if (!g_key_file_has_group (file, group))
        continue;

      window = marker_session_window_new ();
      projects = g_key_file_get_string_list (file, group, "projects", &project_count, NULL);
      documents = g_key_file_get_string_list (file, group, "documents", &document_count, NULL);
      append_string_list (window->projects, projects, project_count);
      append_string_list (window->documents, documents, document_count);
      gint active = g_key_file_get_integer (file, group, "active-project", NULL);
      window->active_project = active < 0 ? G_MAXUINT : active;
      window->active_document = g_key_file_get_string (file, group, "active-document", NULL);
      window->has_navigation = g_key_file_has_key (file, group, "pages-visible", NULL);
      if (window->has_navigation)
        {
          window->pages_visible = g_key_file_get_boolean (file, group, "pages-visible", NULL);
          window->outline_visible = g_key_file_get_boolean (file, group, "outline-visible", NULL);
          window->files_mode = g_key_file_get_boolean (file, group, "files-mode", NULL);
        }
      g_ptr_array_add (windows, window);
    }

  return g_steal_pointer (&windows);
}

gboolean
marker_session_save (GPtrArray *windows,
                     GError   **error)
{
  GKeyFile *file = get_metadata ();
  g_auto (GStrv) groups = NULL;
  gsize group_count = 0;

  g_return_val_if_fail (windows != NULL, FALSE);

  groups = g_key_file_get_groups (file, &group_count);
  for (gsize i = 0; i < group_count; i++)
    if (g_str_equal (groups[i], "Session") || g_str_has_prefix (groups[i], "Window "))
      g_key_file_remove_group (file, groups[i], NULL);

  g_key_file_set_integer (file, "Session", "window-count", windows->len);
  for (guint i = 0; i < windows->len; i++)
    {
      MarkerSessionWindow *window = g_ptr_array_index (windows, i);
      g_autofree char *group = g_strdup_printf ("Window %u", i);

      g_key_file_set_string_list (file, group, "projects",
                                  (const char *const *) window->projects->pdata,
                                  window->projects->len);
      g_key_file_set_string_list (file, group, "documents",
                                  (const char *const *) window->documents->pdata,
                                  window->documents->len);
      g_key_file_set_integer (file, group, "active-project", window->active_project == G_MAXUINT ? -1 : (gint) window->active_project);
      if (window->has_navigation)
        {
          g_key_file_set_boolean (file, group, "pages-visible", window->pages_visible);
          g_key_file_set_boolean (file, group, "outline-visible", window->outline_visible);
          g_key_file_set_boolean (file, group, "files-mode", window->files_mode);
        }
      if (window->active_document != NULL)
        g_key_file_set_string (file, group, "active-document", window->active_document);
    }

  return save_key_file (file, error);
}

void
marker_session_restore_project (MarkerProject *project)
{
  GKeyFile *file = get_metadata ();
  g_autofree char *group = project_group (marker_project_get_uri (project));
  if (!g_key_file_has_group (file, group))
    return;
  g_autofree char *name = g_key_file_get_string (file, group, "name", NULL);
  g_autofree char *icon = g_key_file_get_string (file, group, "icon", NULL);
  g_autofree char *color = g_key_file_get_string (file, group, "color", NULL);
  g_autofree char *text = g_key_file_get_string (file, group, "custom-text", NULL);
  gint mode = g_key_file_has_key (file, group, "icon-mode", NULL)
                ? g_key_file_get_integer (file, group, "icon-mode", NULL)
                : (icon != NULL && *icon != '\0' ? MARKER_PROJECT_ICON_SYMBOL : MARKER_PROJECT_ICON_INITIALS);
  if (name != NULL && *name != '\0' && g_utf8_validate (name, -1, NULL))
    marker_project_set_name (project, name);
  marker_project_set_icon_name (project, icon);
  marker_project_set_color (project, color);
  marker_project_set_custom_text (project, text);
  marker_project_set_icon_mode (project, mode);
}

gboolean
marker_session_store_project (MarkerProject *project, GError **error)
{
  /* Commit the cached metadata only after writing succeeds, like dialog Apply. */
  g_autoptr (GKeyFile) file = g_key_file_new ();
  g_autofree char *data = g_key_file_to_data (get_metadata (), NULL, NULL);
  g_key_file_load_from_data (file, data, -1, G_KEY_FILE_KEEP_COMMENTS, NULL);
  g_autofree char *group = project_group (marker_project_get_uri (project));
  const char *icon = marker_project_get_icon_name (project);
  const char *text = marker_project_get_custom_text (project);
  g_key_file_set_string (file, group, "uri", marker_project_get_uri (project));
  g_key_file_set_string (file, group, "name", marker_project_get_name (project));
  g_key_file_set_string (file, group, "color", marker_project_get_color (project));
  g_key_file_set_integer (file, group, "icon-mode", marker_project_get_icon_mode (project));
  g_key_file_set_string (file, group, "custom-text", text != NULL ? text : "");
  if (icon != NULL)
    g_key_file_set_string (file, group, "icon", icon);
  else
    g_key_file_remove_key (file, group, "icon", NULL);
  if (!save_key_file (file, error))
    return FALSE;
  g_clear_pointer (&metadata, g_key_file_unref);
  metadata = g_steal_pointer (&file);
  return TRUE;
}

void
marker_session_clear (void)
{
  g_autofree char *path = state_path ();
  g_clear_pointer (&metadata, g_key_file_unref);
  if (path != NULL)
    g_unlink (path);
}
