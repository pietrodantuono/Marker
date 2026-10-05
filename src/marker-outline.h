/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include "marker-editor.h"

G_BEGIN_DECLS
#define MARKER_TYPE_OUTLINE (marker_outline_get_type ())
G_DECLARE_FINAL_TYPE (MarkerOutline, marker_outline, MARKER, OUTLINE, GtkBox)
MarkerOutline *marker_outline_new (void);
void marker_outline_set_editor (MarkerOutline *self, MarkerEditor *editor);
G_END_DECLS
