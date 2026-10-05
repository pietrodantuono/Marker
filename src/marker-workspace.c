/*
 * marker-workspace.c
 *
 * Copyright (C) 2026
 *
 * Marker is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 */

#include "marker-workspace.h"
#include "marker-markdown-structure.h"

#include <string.h>

struct _MarkerWorkspaceItem
{
  GObject parent_instance;

  GFile *file;
  char *name;
  char *title;
  char *icon_name;
  char *sort;
  guint64 modified;
  gboolean directory;
  gboolean markdown;
  gboolean enumerated;
  GListStore *children;
};

G_DEFINE_TYPE (MarkerWorkspaceItem, marker_workspace_item, G_TYPE_OBJECT)

#define WORKSPACE_FILE_ATTRIBUTES \
  G_FILE_ATTRIBUTE_STANDARD_NAME "," \
  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
  G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," \
  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
  G_FILE_ATTRIBUTE_STANDARD_ICON "," \
  G_FILE_ATTRIBUTE_TIME_MODIFIED

static gboolean
filename_has_suffix (const gchar *filename,
                     const gchar *suffix)
{
  gsize filename_length = strlen (filename);
  gsize suffix_length = strlen (suffix);

  return filename_length >= suffix_length &&
         g_ascii_strcasecmp (filename + filename_length - suffix_length,
                             suffix) == 0;
}

gboolean
marker_workspace_file_is_markdown (GFileInfo *info)
{
  const gchar *filename;
  const gchar *content_type;

  g_return_val_if_fail (G_IS_FILE_INFO (info), FALSE);

  filename = g_file_info_get_name (info);
  content_type = g_file_info_get_content_type (info);

  return (filename != NULL &&
          (filename_has_suffix (filename, ".md") ||
           filename_has_suffix (filename, ".markdown"))) ||
         (content_type != NULL &&
          (g_content_type_is_a (content_type, "text/markdown") ||
           g_content_type_is_a (content_type, "text/x-markdown")));
}

static gboolean
file_is_visible (GFileInfo *info)
{
  const gchar *filename;
  const gchar *content_type;
  g_autofree gchar *mime_type = NULL;

  filename = g_file_info_get_name (info);
  if ((g_file_info_get_is_hidden (info) && g_strcmp0 (filename, ".marker.css") != 0) ||
      g_file_info_get_is_symlink (info))
    return FALSE;

  if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
    return TRUE;

  if (g_file_info_get_file_type (info) != G_FILE_TYPE_REGULAR)
    return FALSE;

  if (marker_workspace_file_is_markdown (info))
    return TRUE;

  filename = g_file_info_get_name (info);
  if (filename != NULL &&
      (filename_has_suffix (filename, ".csv") ||
       filename_has_suffix (filename, ".dat") ||
       filename_has_suffix (filename, ".css") ||
       filename_has_suffix (filename, ".txt")))
    return TRUE;

  content_type = g_file_info_get_content_type (info);
  if (content_type == NULL)
    return FALSE;

  mime_type = g_content_type_get_mime_type (content_type);
  return mime_type != NULL && g_str_has_prefix (mime_type, "image/");
}

static gint
compare_file_info (gconstpointer left,
                   gconstpointer right)
{
  GFileInfo *left_info = *(GFileInfo * const *) left;
  GFileInfo *right_info = *(GFileInfo * const *) right;
  gboolean left_is_directory;
  gboolean right_is_directory;

  left_is_directory = g_file_info_get_file_type (left_info) == G_FILE_TYPE_DIRECTORY;
  right_is_directory = g_file_info_get_file_type (right_info) == G_FILE_TYPE_DIRECTORY;

  if (left_is_directory != right_is_directory)
    return left_is_directory ? -1 : 1;

  return g_utf8_collate (g_file_info_get_display_name (left_info),
                         g_file_info_get_display_name (right_info));
}

static GPtrArray *
list_children (GFile *directory, GCancellable *cancellable, GError **error)
{
  g_autoptr (GFileEnumerator) enumerator = NULL;
  g_autoptr (GPtrArray) children = NULL;
  g_autoptr (GError) local_error = NULL;
  GFileInfo *info;

  g_return_val_if_fail (G_IS_FILE (directory), NULL);
  g_return_val_if_fail (error == NULL || *error == NULL, NULL);

  enumerator = g_file_enumerate_children (directory,
                                           WORKSPACE_FILE_ATTRIBUTES,
                                           G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                           cancellable,
                                           error);
  if (enumerator == NULL)
    return NULL;

  children = g_ptr_array_new_with_free_func (g_object_unref);

  while ((info = g_file_enumerator_next_file (enumerator,
                                               cancellable,
                                               &local_error)) != NULL)
  {
    if (file_is_visible (info))
      g_ptr_array_add (children, info);
    else
      g_object_unref (info);
  }

  if (local_error != NULL)
  {
    g_propagate_error (error, g_steal_pointer (&local_error));
    return NULL;
  }

  g_ptr_array_sort (children, compare_file_info);
  return g_steal_pointer (&children);
}

GPtrArray *
marker_workspace_list_children (GFile *directory, GError **error)
{
  return list_children (directory, NULL, error);
}

static gboolean
scan_directory (GFile *directory, GPtrArray *items, GCancellable *cancellable, GError **error)
{
  g_autoptr (GPtrArray) infos = list_children (directory, cancellable, error);
  if (infos == NULL)
    return FALSE;
  for (guint i = 0; i < infos->len; i++)
    {
      GFileInfo *info = g_ptr_array_index (infos, i);
      g_autoptr (GFile) file = g_file_get_child (directory, g_file_info_get_name (info));
      g_ptr_array_add (items, marker_workspace_item_new (file, info));
      if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY)
        {
          g_autoptr (GError) child_error = NULL;
          if (!scan_directory (file, items, cancellable, &child_error) &&
              g_error_matches (child_error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
            {
              g_propagate_error (error, g_steal_pointer (&child_error));
              return FALSE;
            }
          /* An unreadable subfolder does not hide the remaining notebook. */
        }
    }
  return !g_cancellable_set_error_if_cancelled (cancellable, error);
}

static void
scan_thread (GTask *task, gpointer source, gpointer data, GCancellable *cancellable)
{
  g_autoptr (GPtrArray) items = g_ptr_array_new_with_free_func (g_object_unref);
  g_autoptr (GError) error = NULL;
  if (scan_directory (G_FILE (source), items, cancellable, &error))
    g_task_return_pointer (task, g_steal_pointer (&items), (GDestroyNotify) g_ptr_array_unref);
  else
    g_task_return_error (task, g_steal_pointer (&error));
}

void
marker_workspace_scan_async (GFile *root, GCancellable *cancellable,
                              GAsyncReadyCallback callback, gpointer user_data)
{
  g_autoptr (GTask) task = g_task_new (root, cancellable, callback, user_data);
  g_task_run_in_thread (task, scan_thread);
}

GPtrArray *
marker_workspace_scan_finish (GAsyncResult *result, GError **error)
{
  return g_task_propagate_pointer (G_TASK (result), error);
}

static char *
read_markdown_title (GFile *file)
{
  g_autoptr (GFileInputStream) stream = NULL;
  char buffer[8193] = { 0 };
  gssize length;

  stream = g_file_read (file, NULL, NULL);
  if (stream == NULL)
    return NULL;
  length = g_input_stream_read (G_INPUT_STREAM (stream), buffer, sizeof buffer - 1,
                                NULL, NULL);
  if (length <= 0)
    return NULL;

  g_autofree char *text = g_utf8_make_valid (buffer, length);
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (text);
  for (guint i = 0; i < structure->headings->len; i++)
    {
      const MarkerHeading *heading = g_ptr_array_index (structure->headings, i);
      if (*heading->title != '\0')
        return g_strdup (heading->title);
    }

  return NULL;
}

static void
marker_workspace_item_dispose (GObject *object)
{
  MarkerWorkspaceItem *item = MARKER_WORKSPACE_ITEM (object);
  g_clear_object (&item->file);
  g_clear_object (&item->children);
  G_OBJECT_CLASS (marker_workspace_item_parent_class)->dispose (object);
}

static void
marker_workspace_item_finalize (GObject *object)
{
  MarkerWorkspaceItem *item = MARKER_WORKSPACE_ITEM (object);
  g_free (item->name);
  g_free (item->title);
  g_free (item->icon_name);
  g_free (item->sort);
  G_OBJECT_CLASS (marker_workspace_item_parent_class)->finalize (object);
}

static void
marker_workspace_item_class_init (MarkerWorkspaceItemClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = marker_workspace_item_dispose;
  object_class->finalize = marker_workspace_item_finalize;
}

static void
marker_workspace_item_init (MarkerWorkspaceItem *item)
{
  item->sort = g_strdup ("name");
}

MarkerWorkspaceItem *
marker_workspace_item_new (GFile     *file,
                           GFileInfo *info)
{
  MarkerWorkspaceItem *item;
  const char *content_type;

  g_return_val_if_fail (G_IS_FILE (file), NULL);
  g_return_val_if_fail (G_IS_FILE_INFO (info), NULL);

  item = g_object_new (MARKER_TYPE_WORKSPACE_ITEM, NULL);
  item->file = g_object_ref (file);
  item->name = g_strdup (g_file_info_get_display_name (info));
  item->directory = g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY;
  item->markdown = marker_workspace_file_is_markdown (info);
  item->modified = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
  content_type = g_file_info_get_content_type (info);

  if (item->directory)
    item->icon_name = g_strdup ("folder-symbolic");
  else if (item->markdown)
    item->icon_name = g_strdup ("text-x-generic-symbolic");
  else if (content_type != NULL && g_str_has_prefix (content_type, "image/"))
    item->icon_name = g_strdup ("image-x-generic-symbolic");
  else
    item->icon_name = g_strdup ("text-x-generic-symbolic");

  item->title = item->markdown ? read_markdown_title (file) : NULL;
  if (item->title == NULL)
    item->title = g_strdup (item->name);
  return item;
}

MarkerWorkspaceItem *
marker_workspace_item_new_root (GFile *directory)
{
  g_autoptr (GFileInfo) info = NULL;

  g_return_val_if_fail (G_IS_FILE (directory), NULL);
  info = g_file_query_info (directory,
                            WORKSPACE_FILE_ATTRIBUTES,
                            G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                            NULL, NULL);
  if (info == NULL)
    {
      info = g_file_info_new ();
      g_autofree char *basename = g_file_get_basename (directory);
      g_file_info_set_name (info, basename != NULL ? basename : "Project");
      g_file_info_set_display_name (info, basename != NULL ? basename : "Project");
      g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
    }
  return marker_workspace_item_new (directory, info);
}

GFile *
marker_workspace_item_get_file (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), NULL);
  return item->file;
}

const char *
marker_workspace_item_get_name (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), NULL);
  return item->name;
}

const char *
marker_workspace_item_get_title (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), NULL);
  return item->title;
}

const char *
marker_workspace_item_get_icon_name (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), NULL);
  return item->icon_name;
}

gboolean
marker_workspace_item_get_directory (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), FALSE);
  return item->directory;
}

gboolean
marker_workspace_item_get_markdown (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), FALSE);
  return item->markdown;
}

guint64
marker_workspace_item_get_modified (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), 0);
  return item->modified;
}

static gint
compare_workspace_items (gconstpointer left,
                         gconstpointer right,
                         gpointer      user_data)
{
  MarkerWorkspaceItem *a = *(MarkerWorkspaceItem *const *) left;
  MarkerWorkspaceItem *b = *(MarkerWorkspaceItem *const *) right;
  const char *sort = user_data;

  if (a->directory != b->directory)
    return a->directory ? -1 : 1;
  if (g_str_equal (sort, "modified") && a->modified != b->modified)
    return a->modified > b->modified ? -1 : 1;
  return g_utf8_collate (a->name, b->name);
}

typedef struct
{
  GFile *file;
  char *sort;
} EnumerationData;

static void
enumeration_data_free (EnumerationData *data)
{
  g_clear_object (&data->file);
  g_free (data->sort);
  g_free (data);
}

static void
enumerate_children_thread (GTask        *task,
                           gpointer      source_object,
                           gpointer      task_data,
                           GCancellable *cancellable)
{
  EnumerationData *data = task_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GPtrArray) infos = list_children (data->file, cancellable, &error);
  GPtrArray *children;

  if (infos == NULL)
    {
      g_task_return_error (task, g_steal_pointer (&error));
      return;
    }

  children = g_ptr_array_new_with_free_func (g_object_unref);
  for (guint i = 0; i < infos->len; i++)
    {
      GFileInfo *info = g_ptr_array_index (infos, i);
      g_autoptr (GFile) child_file = g_file_get_child (data->file,
                                                       g_file_info_get_name (info));
      MarkerWorkspaceItem *child = marker_workspace_item_new (child_file, info);
      g_free (child->sort);
      child->sort = g_strdup (data->sort);
      g_ptr_array_add (children, child);
    }
  g_ptr_array_sort_with_data (children, compare_workspace_items, data->sort);
  g_task_return_pointer (task, children, (GDestroyNotify) g_ptr_array_unref);
}

static void
enumerate_children_done (GObject      *source,
                          GAsyncResult *result,
                          gpointer      user_data)
{
  g_autoptr (GListStore) store = G_LIST_STORE (user_data);
  g_autoptr (GError) error = NULL;
  g_autoptr (GPtrArray) children = g_task_propagate_pointer (G_TASK (result), &error);

  if (children == NULL)
    return;
  for (guint i = 0; i < children->len; i++)
    g_list_store_append (store, g_ptr_array_index (children, i));
}

GListModel *
marker_workspace_item_get_children (MarkerWorkspaceItem *item)
{
  g_return_val_if_fail (MARKER_IS_WORKSPACE_ITEM (item), NULL);
  if (!item->directory)
    return NULL;

  if (item->children == NULL)
    item->children = g_list_store_new (MARKER_TYPE_WORKSPACE_ITEM);
  if (!item->enumerated)
    {
      EnumerationData *data = g_new0 (EnumerationData, 1);
      g_autoptr (GTask) task = g_task_new (NULL, NULL, enumerate_children_done,
                                           g_object_ref (item->children));
      data->file = g_object_ref (item->file);
      data->sort = g_strdup (item->sort);
      g_task_set_task_data (task, data, (GDestroyNotify) enumeration_data_free);
      item->enumerated = TRUE;
      g_task_run_in_thread (task, enumerate_children_thread);
    }
  return G_LIST_MODEL (item->children);
}

void
marker_workspace_item_set_sort (MarkerWorkspaceItem *item,
                                const char          *sort)
{
  g_return_if_fail (MARKER_IS_WORKSPACE_ITEM (item));
  g_free (item->sort);
  item->sort = g_strdup (g_str_equal (sort, "modified") ? "modified" : "name");
}
