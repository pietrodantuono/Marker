/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MARKER_NOTEBOOK_RAIL_H
#define MARKER_NOTEBOOK_RAIL_H

#include <gtk/gtk.h>
#include "marker-project.h"

G_BEGIN_DECLS

#define MARKER_TYPE_NOTEBOOK_RAIL (marker_notebook_rail_get_type ())
G_DECLARE_FINAL_TYPE (MarkerNotebookRail, marker_notebook_rail, MARKER, NOTEBOOK_RAIL, GtkBox)

GtkWidget *marker_notebook_rail_new (GtkSingleSelection *selection);

G_END_DECLS
#endif
