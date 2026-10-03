/*
 * marker-gnuplot.c
 *
 * Copyright (C) 2026 Marker contributors
 *
 * This file is part of Marker and is distributed under the GNU General
 * Public License version 3 or later.
 */

#define _GNU_SOURCE

#include "marker-gnuplot.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

static GBytes *
fail (GError      **error,
      GIOErrorEnum  code,
      const gchar  *message)
{
  g_set_error_literal (error, G_IO_ERROR, code, message);
  return NULL;
}

static gboolean
valid_operand (const gchar *operand)
{
  const gchar *extension;
  g_auto (GStrv) parts = NULL;

  if (operand == NULL || *operand == '\0' || !g_utf8_validate (operand, -1, NULL) ||
      g_path_is_absolute (operand) || operand[0] == '~' || operand[0] == '<' ||
      strchr (operand, '\\') != NULL || strchr (operand, ':') != NULL)
    return FALSE;

  for (const guchar *cursor = (const guchar *) operand; *cursor != '\0'; cursor++)
    if (*cursor < 0x20 || *cursor == 0x7f)
      return FALSE;

  parts = g_strsplit (operand, "/", -1);
  for (guint i = 0; parts[i] != NULL; i++)
    if (*parts[i] == '\0' || g_str_equal (parts[i], ".."))
      return FALSE;

  extension = strrchr (operand, '.');
  return extension != NULL &&
         (g_ascii_strcasecmp (extension, ".csv") == 0 ||
          g_ascii_strcasecmp (extension, ".dat") == 0 ||
          g_ascii_strcasecmp (extension, ".txt") == 0);
}

static gboolean
same_file_state (const struct stat *left,
                 const struct stat *right)
{
  return left->st_dev == right->st_dev &&
         left->st_ino == right->st_ino &&
         left->st_size == right->st_size &&
         left->st_mtim.tv_sec == right->st_mtim.tv_sec &&
         left->st_mtim.tv_nsec == right->st_mtim.tv_nsec &&
         left->st_ctim.tv_sec == right->st_ctim.tv_sec &&
         left->st_ctim.tv_nsec == right->st_ctim.tv_nsec;
}

GBytes *
marker_gnuplot_load_data (const gchar  *document_dir,
                          const gchar  *operand,
                          GFile       **file,
                          GError      **error)
{
  g_autofree gchar *root = NULL;
  g_autofree gchar *target = NULL;
  g_auto (GStrv) parts = NULL;
  struct stat before;
  struct stat after;
  guint8 *contents = NULL;
  gsize length;
  gsize offset = 0;
  gint descriptor = -1;

  g_return_val_if_fail (file == NULL || *file == NULL, NULL);

  if (document_dir == NULL || *document_dir == '\0')
    return fail (error, G_IO_ERROR_NOT_SUPPORTED,
                 "Save this Markdown document before using plot data files.");
  if (!valid_operand (operand))
    return fail (error, G_IO_ERROR_INVALID_ARGUMENT,
                 "Data files must be relative .csv, .dat, or .txt paths below this Markdown document.");

  root = realpath (document_dir, NULL);
  if (root == NULL)
    return fail (error, G_IO_ERROR_NOT_FOUND,
                 "The Markdown document folder is unavailable.");

  target = g_canonicalize_filename (operand, root);
  if (file != NULL)
    *file = g_file_new_for_path (target);

  parts = g_strsplit (operand, "/", -1);
  descriptor = open (root, O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
  if (descriptor < 0)
    return fail (error, G_IO_ERROR_NOT_FOUND,
                 "The Markdown document folder is unavailable.");

  for (guint i = 0; parts[i] != NULL; i++) {
    gint next;
    gint flags = O_RDONLY | O_CLOEXEC | O_NOFOLLOW;
    gint open_error;

    if (g_str_equal (parts[i], "."))
      continue;
    if (parts[i + 1] != NULL)
      flags |= O_DIRECTORY;
    next = openat (descriptor, parts[i], flags);
    open_error = errno;
    close (descriptor);
    if (next < 0)
      return fail (error,
                   open_error == ELOOP ? G_IO_ERROR_PERMISSION_DENIED
                                       : G_IO_ERROR_NOT_FOUND,
                   open_error == ELOOP ? "Symlink plot data paths are not supported."
                                       : "Unable to read the plot data file.");
    descriptor = next;
  }

  if (fstat (descriptor, &before) != 0 || !S_ISREG (before.st_mode)) {
    close (descriptor);
    return fail (error, G_IO_ERROR_NOT_REGULAR_FILE,
                 "Plot data must be a regular file.");
  }
  if (before.st_size < 0 || before.st_size > MARKER_GNUPLOT_MAX_FILE_BYTES) {
    close (descriptor);
    return fail (error, G_IO_ERROR_NO_SPACE,
                 "A plot data file exceeds the 1 MB limit.");
  }

  length = (gsize) before.st_size;
  contents = g_malloc (MAX (length, 1));
  while (offset < length) {
    ssize_t count = read (descriptor, contents + offset, length - offset);
    if (count < 0 && errno == EINTR)
      continue;
    if (count <= 0)
      break;
    offset += (gsize) count;
  }

  guint8 extra;
  ssize_t extra_count;
  do {
    extra_count = read (descriptor, &extra, 1);
  } while (extra_count < 0 && errno == EINTR);

  gboolean unchanged = fstat (descriptor, &after) == 0 &&
                       same_file_state (&before, &after) &&
                       offset == length && extra_count == 0;
  close (descriptor);

  if (!unchanged) {
    g_free (contents);
    return fail (error, G_IO_ERROR_BUSY,
                 "The plot data file changed while it was being read.");
  }

  return g_bytes_new_take (contents, length);
}
