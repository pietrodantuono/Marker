#include "marker-workspace.h"

#include <glib/gstdio.h>
#include <unistd.h>

static void
remove_test_entry (const gchar *directory,
                   const gchar *name)
{
  g_autofree gchar *path = g_build_filename (directory, name, NULL);
  g_remove (path);
}

static void
test_workspace_listing (void)
{
  const gchar *expected[] = {
    "alpha-dir", "zeta", "alpha.md", "BETA.MARKDOWN", "chart.png",
    "notes.txt", "points.DAT", "values.csv", ".marker.css"
  };
  g_autofree gchar *temporary = g_dir_make_tmp ("marker-workspace-XXXXXX", NULL);
  g_autofree gchar *regular_file = NULL;
  g_autofree gchar *symlink_path = NULL;
  g_autoptr (GFile) root = NULL;
  g_autoptr (GFile) not_directory = NULL;
  g_autoptr (GPtrArray) children = NULL;
  g_autoptr (GHashTable) names = NULL;
  g_autoptr (GError) error = NULL;

  g_assert_nonnull (temporary);
  root = g_file_new_for_path (temporary);
  names = g_hash_table_new (g_str_hash, g_str_equal);

  for (guint i = 0; i < G_N_ELEMENTS (expected); i++)
  {
    g_autofree gchar *path = g_build_filename (temporary, expected[i], NULL);

    if (i < 2)
      g_assert_cmpint (g_mkdir (path, 0700), ==, 0);
    else
      g_assert_true (g_file_set_contents (path, "test", -1, NULL));
  }

  regular_file = g_build_filename (temporary, "ignore.json", NULL);
  g_assert_true (g_file_set_contents (regular_file, "{}", -1, NULL));
  {
    g_autofree gchar *hidden = g_build_filename (temporary, ".hidden.md", NULL);
    g_assert_true (g_file_set_contents (hidden, "hidden", -1, NULL));
  }
  symlink_path = g_build_filename (temporary, "linked.md", NULL);
  g_assert_cmpint (symlink (regular_file, symlink_path), ==, 0);

  children = marker_workspace_list_children (root, &error);
  g_assert_no_error (error);
  g_assert_cmpuint (children->len, ==, G_N_ELEMENTS (expected));
  g_assert_cmpstr (g_file_info_get_name (g_ptr_array_index (children, 0)),
                   ==, "alpha-dir");
  g_assert_cmpstr (g_file_info_get_name (g_ptr_array_index (children, 1)),
                   ==, "zeta");

  for (guint i = 0; i < children->len; i++)
  {
    GFileInfo *info = g_ptr_array_index (children, i);
    const gchar *name = g_file_info_get_name (info);

    g_hash_table_add (names, (gpointer) name);
    if (i < 2)
      g_assert_cmpint (g_file_info_get_file_type (info), ==, G_FILE_TYPE_DIRECTORY);
    else
      g_assert_cmpint (g_file_info_get_file_type (info), ==, G_FILE_TYPE_REGULAR);

    if (g_str_equal (name, "alpha.md") || g_str_equal (name, "BETA.MARKDOWN"))
      g_assert_true (marker_workspace_file_is_markdown (info));
  }

  for (guint i = 0; i < G_N_ELEMENTS (expected); i++)
    g_assert_true (g_hash_table_contains (names, expected[i]));
  g_assert_false (g_hash_table_contains (names, "ignore.json"));
  g_assert_false (g_hash_table_contains (names, ".hidden.md"));
  g_assert_false (g_hash_table_contains (names, "linked.md"));

  not_directory = g_file_new_for_path (regular_file);
  g_clear_pointer (&children, g_ptr_array_unref);
  children = marker_workspace_list_children (not_directory, &error);
  g_assert_null (children);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY);

  g_remove (symlink_path);
  remove_test_entry (temporary, ".hidden.md");
  remove_test_entry (temporary, "ignore.json");
  for (guint i = 2; i < G_N_ELEMENTS (expected); i++)
    remove_test_entry (temporary, expected[i]);
  for (guint i = 0; i < 2; i++)
  {
    g_autofree gchar *path = g_build_filename (temporary, expected[i], NULL);
    g_rmdir (path);
  }
  g_rmdir (temporary);
}

typedef struct
{
  GMainLoop *loop;
  gboolean loaded;
} AsyncResult;

static void
children_changed_cb (GListModel  *model,
                     guint        position,
                     guint        removed,
                     guint        added,
                     AsyncResult *result)
{
  if (g_list_model_get_n_items (model) > 0)
    {
      result->loaded = TRUE;
      g_main_loop_quit (result->loop);
    }
}

static gboolean
async_timeout_cb (gpointer user_data)
{
  g_main_loop_quit (((AsyncResult *) user_data)->loop);
  return G_SOURCE_REMOVE;
}

static void
test_workspace_async_model_lifetime (void)
{
  g_autofree char *temporary = g_dir_make_tmp ("marker-workspace-async-XXXXXX", NULL);
  g_autofree char *document = g_build_filename (temporary, "notes.md", NULL);
  g_autoptr (GFile) root_file = NULL;
  g_autoptr (MarkerWorkspaceItem) root = NULL;
  g_autoptr (GListModel) children = NULL;
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  AsyncResult result = { loop, FALSE };
  guint timeout;

  g_assert_nonnull (temporary);
  g_assert_true (g_file_set_contents (document, "# Notes\n", -1, NULL));
  root_file = g_file_new_for_path (temporary);
  root = marker_workspace_item_new_root (root_file);
  children = g_object_ref (marker_workspace_item_get_children (root));
  g_signal_connect (children, "items-changed", G_CALLBACK (children_changed_cb),
                    &result);

  /* The tree model owns the child store; loading must remain safe after the
   * temporary root object used to create it has gone away. */
  g_clear_object (&root);
  timeout = g_timeout_add_seconds (2, async_timeout_cb, &result);
  g_main_loop_run (loop);
  if (g_main_context_find_source_by_id (NULL, timeout) != NULL)
    g_source_remove (timeout);

  g_assert_true (result.loaded);
  g_assert_cmpuint (g_list_model_get_n_items (children), ==, 1);
  g_remove (document);
  g_rmdir (temporary);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/workspace/listing", test_workspace_listing);
  g_test_add_func ("/workspace/async-model-lifetime",
                   test_workspace_async_model_lifetime);
  return g_test_run ();
}
