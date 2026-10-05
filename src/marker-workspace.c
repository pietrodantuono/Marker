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

#include <string.h>

#define WORKSPACE_FILE_ATTRIBUTES \
  G_FILE_ATTRIBUTE_STANDARD_NAME "," \
  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME "," \
  G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
  G_FILE_ATTRIBUTE_STANDARD_IS_HIDDEN "," \
  G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," \
  G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE "," \
  G_FILE_ATTRIBUTE_STANDARD_ICON

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

  if (g_file_info_get_is_hidden (info) ||
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

GPtrArray *
marker_workspace_list_children (GFile   *directory,
                                GError **error)
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
                                           NULL,
                                           error);
  if (enumerator == NULL)
    return NULL;

  children = g_ptr_array_new_with_free_func (g_object_unref);

  /* ponytail: synchronous one-level reads keep this small; use async reads if
   * expanding very large or remote folders becomes a measured problem. */
  while ((info = g_file_enumerator_next_file (enumerator,
                                               NULL,
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
