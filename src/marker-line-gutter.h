/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MARKER_LINE_GUTTER_H
#define MARKER_LINE_GUTTER_H
#include <gtksourceview/gtksource.h>
G_BEGIN_DECLS
#define MARKER_TYPE_LINE_GUTTER (marker_line_gutter_get_type ())
G_DECLARE_FINAL_TYPE (MarkerLineGutter, marker_line_gutter, MARKER, LINE_GUTTER, GtkSourceGutterRendererText)
GtkSourceGutterRenderer *marker_line_gutter_new (void);
G_END_DECLS
#endif
