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

GPtrArray           *marker_workspace_list_children              (GFile              *directory,
                                                                  GError            **error);
gboolean             marker_workspace_file_is_markdown           (GFileInfo          *info);

G_END_DECLS

#endif
