/*
 * marker-gnuplot.h
 *
 * Copyright (C) 2026 Marker contributors
 *
 * This file is part of Marker and is distributed under the GNU General
 * Public License version 3 or later.
 */

#ifndef __MARKER_GNUPLOT_H__
#define __MARKER_GNUPLOT_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define MARKER_GNUPLOT_MAX_FILE_BYTES (1024 * 1024)

GBytes *marker_gnuplot_load_data (const gchar  *document_dir,
                                  const gchar  *operand,
                                  GFile       **file,
                                  GError      **error);

G_END_DECLS

#endif
