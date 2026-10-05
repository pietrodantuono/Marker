#include "marker-project.h"

#include <glib/gstdio.h>

static void
test_project_identity_and_containment (void)
{
  g_autofree char *temporary = g_dir_make_tmp ("marker-project-XXXXXX", NULL);
  g_autofree char *folder_path = g_build_filename (temporary, "alpha project", NULL);
  g_autofree char *nested_path = g_build_filename (folder_path, "notes.md", NULL);
  g_autofree char *equivalent_path = g_build_filename (folder_path, "..", "alpha project", NULL);
  g_autofree char *outside_path = g_build_filename (temporary, "outside.md", NULL);
  g_autoptr (GFile) folder = NULL;
  g_autoptr (GFile) equivalent = NULL;
  g_autoptr (GFile) nested = NULL;
  g_autoptr (GFile) outside = NULL;
  g_autoptr (MarkerProject) project = NULL;
  g_autoptr (MarkerProject) duplicate = NULL;

  g_assert_nonnull (temporary);
  g_assert_cmpint (g_mkdir (folder_path, 0700), ==, 0);
  g_assert_true (g_file_set_contents (nested_path, "# Notes\n", -1, NULL));
  g_assert_true (g_file_set_contents (outside_path, "# Outside\n", -1, NULL));

  folder = g_file_new_for_path (folder_path);
  equivalent = g_file_new_for_path (equivalent_path);
  nested = g_file_new_for_path (nested_path);
  outside = g_file_new_for_path (outside_path);
  project = marker_project_new (folder);
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_INITIALS);
  duplicate = marker_project_new (equivalent);

  g_assert_cmpstr (marker_project_get_name (project), ==, "alpha project");
  g_assert_cmpstr (marker_project_get_text (project), ==, "AP");
  g_assert_cmpstr (marker_project_get_uri (project), ==,
                   marker_project_get_uri (duplicate));
  g_assert_cmpstr (marker_project_get_color (project), ==,
                   marker_project_get_color (duplicate));
  g_assert_true (marker_project_contains (project, nested));
  g_assert_false (marker_project_contains (project, outside));
  g_assert_false (marker_project_get_missing (project));

  marker_project_set_name (project, "Ångström Project");
  g_assert_cmpstr (marker_project_get_text (project), ==, "ÅP");

  g_remove (nested_path);
  g_remove (outside_path);
  g_rmdir (folder_path);
  g_rmdir (temporary);
}

static void
test_appearance_modes (void)
{
  g_autoptr (GFile) root = g_file_new_for_path ("/tmp/notebook");
  g_autoptr (MarkerProject) project = marker_project_new (root);
  marker_project_set_name (project, "ÅngströmProject");
  g_assert_cmpstr (marker_project_get_text (project), ==, "Ån");
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_CAMEL);
  g_assert_cmpstr (marker_project_get_text (project), ==, "ÅP");
  marker_project_set_name (project, "alpha_project");
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_SNAKE);
  g_assert_cmpstr (marker_project_get_text (project), ==, "AP");
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_SYMBOL);
  marker_project_set_icon_name (project, "icon-science-symbolic");
  g_assert_cmpstr (marker_project_get_icon_name (project), ==, "icon-science-symbolic");
  marker_project_set_custom_text (project, "研究");
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_CUSTOM);
  g_assert_cmpstr (marker_project_get_text (project), ==, "研究");
  marker_project_set_color (project, "purple");
  g_assert_cmpstr (marker_project_get_color (project), ==, "#9141ac");
  marker_project_set_color (project, "#FFFFFF");
  g_assert_cmpstr (marker_project_get_color (project), ==, "#ffffff");
  g_assert_cmpstr (marker_project_get_foreground (project), ==, "#000000");
  marker_project_set_color (project, "invalid");
  g_assert_cmpstr (marker_project_get_color (project), ==, "#ffffff");
  marker_project_set_color (project, "#000000");
  g_assert_cmpstr (marker_project_get_foreground (project), ==, "#ffffff");
  g_autoptr (MarkerProject) draft = marker_project_new (root);
  marker_project_copy_appearance (draft, project);
  marker_project_set_name (draft, "Draft");
  g_assert_cmpstr (marker_project_get_name (project), ==, "alpha_project");
}

static void
test_stylesheet_creation (void)
{
  g_autofree char *path = g_dir_make_tmp ("marker-styles-XXXXXX", NULL);
  g_autoptr (GFile) root = g_file_new_for_path (path);
  g_autoptr (MarkerProject) project = marker_project_new (root);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = marker_project_ensure_stylesheet (project, &error);
  g_assert_no_error (error);
  g_assert_nonnull (file);
  const char *custom = "body { color: red; }";
  g_assert_true (g_file_replace_contents (file, custom, strlen (custom), NULL, FALSE,
                                         G_FILE_CREATE_NONE, NULL, NULL, &error));
  g_autoptr (GFile) existing = marker_project_ensure_stylesheet (project, &error);
  g_assert_no_error (error);
  g_assert_true (g_file_equal (file, existing));
  g_autofree char *contents = NULL;
  g_assert_true (g_file_load_contents (existing, NULL, &contents, NULL, NULL, &error));
  g_assert_cmpstr (contents, ==, custom);
  g_file_delete (file, NULL, NULL);
  g_file_make_directory (file, NULL, NULL);
  g_clear_object (&existing);
  existing = marker_project_ensure_stylesheet (project, &error);
  g_assert_null (existing);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_REGULAR_FILE);
  g_file_delete (file, NULL, NULL);
  g_file_delete (root, NULL, NULL);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/project/identity-and-containment",
                   test_project_identity_and_containment);
  g_test_add_func ("/project/appearance", test_appearance_modes);
  g_test_add_func ("/project/styles-create-preserve", test_stylesheet_creation);
  return g_test_run ();
}
