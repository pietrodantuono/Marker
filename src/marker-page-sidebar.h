/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MARKER_PAGE_SIDEBAR_H
#define MARKER_PAGE_SIDEBAR_H

#include <adwaita.h>
#include "marker-editor.h"

G_BEGIN_DECLS
#define MARKER_TYPE_PAGE_SIDEBAR (marker_page_sidebar_get_type ())
G_DECLARE_FINAL_TYPE (MarkerPageSidebar, marker_page_sidebar, MARKER, PAGE_SIDEBAR, GtkBox)

GtkWidget *marker_page_sidebar_new          (GListModel *documents);
void       marker_page_sidebar_set_project  (MarkerPageSidebar *self, MarkerProject *project);
void       marker_page_sidebar_set_active   (MarkerPageSidebar *self, MarkerEditor *editor);
void       marker_page_sidebar_refresh      (MarkerPageSidebar *self);
gboolean   marker_page_sidebar_get_files    (MarkerPageSidebar *self);
void       marker_page_sidebar_set_files    (MarkerPageSidebar *self, gboolean files);

G_END_DECLS
#endif
