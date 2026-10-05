/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <gtksourceview/gtksource.h>

G_BEGIN_DECLS
#define MARKER_TYPE_HEADING_GUTTER (marker_heading_gutter_get_type ())
G_DECLARE_FINAL_TYPE (MarkerHeadingGutter, marker_heading_gutter, MARKER, HEADING_GUTTER, GtkSourceGutterRendererText)
GtkSourceGutterRenderer *marker_heading_gutter_new (void);
G_END_DECLS
