#include "marker-gnuplot.h"

#include <glib/gstdio.h>
#include <unistd.h>

static void
test_data_file_boundary (void)
{
  g_autofree gchar *temporary = g_dir_make_tmp ("marker-gnuplot-XXXXXX", NULL);
  g_autofree gchar *root = g_build_filename (temporary, "document", NULL);
  g_autofree gchar *subdir = g_build_filename (root, "data", NULL);
  g_autofree gchar *valid_path = g_build_filename (subdir, "values.csv", NULL);
  g_autofree gchar *outside_path = g_build_filename (temporary, "outside.csv", NULL);
  g_autofree gchar *symlink_path = g_build_filename (root, "linked.csv", NULL);
  g_autofree gchar *symlink_dir = g_build_filename (root, "linked-data", NULL);
  g_autofree gchar *large_path = g_build_filename (root, "large.dat", NULL);
  g_autofree gchar *large = g_malloc0 (MARKER_GNUPLOT_MAX_FILE_BYTES + 1);
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;
  g_autoptr (GBytes) bytes = NULL;

  g_assert_nonnull (temporary);
  g_assert_cmpint (g_mkdir_with_parents (subdir, 0700), ==, 0);
  g_assert_true (g_file_set_contents (valid_path, "x,y\n0,1\n", -1, NULL));
  g_assert_true (g_file_set_contents (outside_path, "0,2\n", -1, NULL));
  g_assert_true (g_file_set_contents (large_path, large,
                                      MARKER_GNUPLOT_MAX_FILE_BYTES + 1, NULL));
  g_assert_cmpint (symlink (outside_path, symlink_path), ==, 0);
  g_assert_cmpint (symlink (subdir, symlink_dir), ==, 0);

  bytes = marker_gnuplot_load_data (root, "data/values.csv", &file, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (g_bytes_get_size (bytes), ==, 8);

  g_clear_pointer (&bytes, g_bytes_unref);
  g_clear_object (&file);
  bytes = marker_gnuplot_load_data (root, "../outside.csv", &file, &error);
  g_assert_null (bytes);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);

  g_clear_error (&error);
  g_clear_object (&file);
  bytes = marker_gnuplot_load_data (root, outside_path, &file, &error);
  g_assert_null (bytes);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);

  g_clear_error (&error);
  g_clear_object (&file);
  bytes = marker_gnuplot_load_data (root, "data/values.json", &file, &error);
  g_assert_null (bytes);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);

  g_clear_error (&error);
  g_clear_object (&file);
  bytes = marker_gnuplot_load_data (root, "linked.csv", &file, &error);
  g_assert_null (bytes);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED);

  g_clear_error (&error);
  g_clear_object (&file);
  bytes = marker_gnuplot_load_data (root, "linked-data/values.csv", &file, &error);
  g_assert_null (bytes);
  g_assert_nonnull (error);

  g_clear_error (&error);
  g_clear_object (&file);
  bytes = marker_gnuplot_load_data (root, "large.dat", &file, &error);
  g_assert_null (bytes);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NO_SPACE);

  g_remove (symlink_path);
  g_remove (symlink_dir);
  g_remove (large_path);
  g_remove (valid_path);
  g_remove (outside_path);
  g_rmdir (subdir);
  g_rmdir (root);
  g_rmdir (temporary);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/gnuplot/data-file-boundary", test_data_file_boundary);
  return g_test_run ();
}
