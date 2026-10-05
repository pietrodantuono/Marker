/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MARKER_NOTEBOOK_ICON_H
#define MARKER_NOTEBOOK_ICON_H
#include <gtk/gtk.h>
#include "marker-project.h"
G_BEGIN_DECLS
#define MARKER_TYPE_NOTEBOOK_ICON (marker_notebook_icon_get_type ())
G_DECLARE_FINAL_TYPE (MarkerNotebookIcon, marker_notebook_icon, MARKER, NOTEBOOK_ICON, GtkBox)
GtkWidget *marker_notebook_icon_new          (MarkerProject *project);
void       marker_notebook_icon_set_project  (MarkerNotebookIcon *self, MarkerProject *project);
void       marker_notebook_icon_set_selected (MarkerNotebookIcon *self, gboolean selected);
G_END_DECLS
#endif
