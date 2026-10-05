/*
 * marker-workspace.h
 *
 * Copyright (C) 2026
 *
 * Marker is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 */

#ifndef __MARKER_WORKSPACE_H__
#define __MARKER_WORKSPACE_H__

#include <gio/gio.h>

G_BEGIN_DECLS

#define MARKER_TYPE_WORKSPACE_ITEM (marker_workspace_item_get_type ())
G_DECLARE_FINAL_TYPE (MarkerWorkspaceItem, marker_workspace_item, MARKER, WORKSPACE_ITEM, GObject)

GPtrArray           *marker_workspace_list_children              (GFile              *directory,
                                                                  GError            **error);
void                 marker_workspace_scan_async                 (GFile              *root,
                                                                  GCancellable       *cancellable,
                                                                  GAsyncReadyCallback callback,
                                                                  gpointer            user_data);
GPtrArray           *marker_workspace_scan_finish                (GAsyncResult       *result,
                                                                  GError            **error);
gboolean             marker_workspace_file_is_markdown           (GFileInfo          *info);
MarkerWorkspaceItem *marker_workspace_item_new                    (GFile              *file,
                                                                  GFileInfo          *info);
MarkerWorkspaceItem *marker_workspace_item_new_root               (GFile              *directory);
GFile               *marker_workspace_item_get_file               (MarkerWorkspaceItem *item);
const char          *marker_workspace_item_get_name               (MarkerWorkspaceItem *item);
const char          *marker_workspace_item_get_title              (MarkerWorkspaceItem *item);
const char          *marker_workspace_item_get_icon_name          (MarkerWorkspaceItem *item);
gboolean             marker_workspace_item_get_directory          (MarkerWorkspaceItem *item);
gboolean             marker_workspace_item_get_markdown           (MarkerWorkspaceItem *item);
guint64              marker_workspace_item_get_modified           (MarkerWorkspaceItem *item);
GListModel          *marker_workspace_item_get_children           (MarkerWorkspaceItem *item);
void                 marker_workspace_item_set_sort               (MarkerWorkspaceItem *item,
                                                                  const char          *sort);

G_END_DECLS

#endif
