#include "marker-session.h"

#include <glib/gstdio.h>

static void
test_legacy_appearance (void)
{
  g_autofree char *temporary = g_dir_make_tmp ("marker-legacy-XXXXXX", NULL);
  g_setenv ("MARKER_STATE_DIR", temporary, TRUE);
  marker_session_clear ();
  g_autofree char *directory = g_build_filename (temporary, "marker", NULL);
  g_autofree char *path = g_build_filename (directory, "session.ini", NULL);
  g_autoptr (GFile) root = g_file_new_for_path ("/tmp/legacy-notebook");
  g_autofree char *uri = g_file_get_uri (root);
  g_autofree char *digest = g_compute_checksum_for_string (G_CHECKSUM_SHA256, uri, -1);
  g_autofree char *group = g_strdup_printf ("Project %s", digest);
  g_autoptr (GKeyFile) file = g_key_file_new ();
  g_key_file_set_string (file, group, "name", "Legacy Notebook");
  g_key_file_set_string (file, group, "color", "green");
  g_key_file_set_string (file, group, "icon", "folder-symbolic");
  g_assert_true (g_key_file_save_to_file (file, path, NULL));
  g_autoptr (GPtrArray) windows = marker_session_load (NULL);
  g_autoptr (MarkerProject) project = marker_project_new (root);
  marker_session_restore_project (project);
  g_assert_cmpint (marker_project_get_icon_mode (project), ==, MARKER_PROJECT_ICON_SYMBOL);
  g_assert_cmpstr (marker_project_get_icon_name (project), ==, "folder-symbolic");
  g_assert_cmpstr (marker_project_get_color (project), ==, "#2ec27e");
  g_key_file_remove_key (file, group, "icon", NULL);
  g_assert_true (g_key_file_save_to_file (file, path, NULL));
  g_clear_pointer (&windows, g_ptr_array_unref);
  windows = marker_session_load (NULL);
  marker_session_restore_project (project);
  g_assert_cmpint (marker_project_get_icon_mode (project), ==, MARKER_PROJECT_ICON_INITIALS);
  g_assert_cmpstr (marker_project_get_text (project), ==, "LN");
  g_key_file_set_integer (file, group, "icon-mode", 999);
  g_key_file_set_string (file, group, "color", "malformed");
  g_assert_true (g_key_file_save_to_file (file, path, NULL));
  g_clear_pointer (&windows, g_ptr_array_unref);
  windows = marker_session_load (NULL);
  marker_session_restore_project (project);
  g_assert_cmpint (marker_project_get_icon_mode (project), ==, MARKER_PROJECT_ICON_INITIALS);
  g_assert_cmpstr (marker_project_get_color (project), ==, "#2ec27e");
  marker_session_clear ();
  g_rmdir (directory);
  g_rmdir (temporary);
  g_unsetenv ("MARKER_STATE_DIR");
}

static void
test_session_round_trip (void)
{
  g_autofree char *temporary = g_dir_make_tmp ("marker-session-XXXXXX", NULL);
  g_autoptr (GPtrArray) windows = NULL;
  g_autoptr (GPtrArray) loaded = NULL;
  g_autoptr (GError) error = NULL;
  MarkerSessionWindow *window;
  MarkerSessionWindow *restored;
  g_autoptr (GFile) root = g_file_new_for_uri ("file:///tmp/project");
  g_autoptr (MarkerProject) project = marker_project_new (root);
  g_autofree char *state_directory = NULL;

  g_assert_nonnull (temporary);
  g_setenv ("MARKER_STATE_DIR", temporary, TRUE);
  marker_session_clear ();

  windows = g_ptr_array_new_with_free_func (
    (GDestroyNotify) marker_session_window_free);
  window = marker_session_window_new ();
  g_ptr_array_add (window->projects, g_strdup ("file:///tmp/project"));
  g_ptr_array_add (window->documents, g_strdup ("file:///tmp/project/notes.md"));
  window->active_project = 0;
  window->has_navigation = TRUE;
  window->pages_visible = FALSE;
  window->outline_visible = TRUE;
  window->files_mode = TRUE;
  window->active_document = g_strdup ("file:///tmp/project/notes.md");
  g_ptr_array_add (windows, window);

  marker_project_set_name (project, "Research");
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_CUSTOM);
  marker_project_set_custom_text (project, "AB");
  marker_project_set_icon_name (project, "folder-documents-symbolic");
  marker_project_set_color (project, "purple");
  g_assert_true (marker_session_store_project (project, &error));
  g_assert_no_error (error);
  g_assert_true (marker_session_save (windows, &error));
  g_assert_no_error (error);

  loaded = marker_session_load (&error);
  g_assert_no_error (error);
  g_assert_cmpuint (loaded->len, ==, 1);
  restored = g_ptr_array_index (loaded, 0);
  g_assert_cmpuint (restored->projects->len, ==, 1);
  g_assert_cmpuint (restored->documents->len, ==, 1);
  g_assert_cmpstr (g_ptr_array_index (restored->projects, 0), ==,
                   "file:///tmp/project");
  g_assert_cmpstr (restored->active_document, ==,
                   "file:///tmp/project/notes.md");
  g_assert_true (restored->has_navigation);
  g_assert_false (restored->pages_visible);
  g_assert_true (restored->outline_visible);
  g_assert_true (restored->files_mode);

  g_autoptr (MarkerProject) restored_project = marker_project_new (root);
  marker_session_restore_project (restored_project);
  g_assert_cmpstr (marker_project_get_name (restored_project), ==, "Research");
  g_assert_cmpstr (marker_project_get_icon_name (restored_project), ==, "folder-documents-symbolic");
  g_assert_cmpstr (marker_project_get_color (restored_project), ==, "#9141ac");
  g_assert_cmpstr (marker_project_get_text (restored_project), ==, "AB");
  g_assert_cmpint (marker_project_get_icon_mode (restored_project), ==, MARKER_PROJECT_ICON_CUSTOM);

  marker_session_clear ();
  state_directory = g_build_filename (temporary, "marker", NULL);
  g_rmdir (state_directory);
  g_rmdir (temporary);
  g_unsetenv ("MARKER_STATE_DIR");
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/session/round-trip", test_session_round_trip);
  g_test_add_func ("/session/legacy-appearance", test_legacy_appearance);
  return g_test_run ();
}
