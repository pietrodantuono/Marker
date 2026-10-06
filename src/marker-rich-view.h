/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MARKER_RICH_VIEW_H
#define MARKER_RICH_VIEW_H
#include "marker-source-view.h"
G_BEGIN_DECLS
#define MARKER_TYPE_RICH_VIEW (marker_rich_view_get_type ())
G_DECLARE_FINAL_TYPE (MarkerRichView, marker_rich_view, MARKER, RICH_VIEW, GObject)
MarkerRichView *marker_rich_view_new (MarkerSourceView *source);
void marker_rich_view_set_enabled (MarkerRichView *self, gboolean enabled);
void marker_rich_view_refresh (MarkerRichView *self, const char *theme,
                               const char *document_path, const char *notebook_folder);
G_END_DECLS
#endif
