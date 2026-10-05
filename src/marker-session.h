/* marker-session.h
 *
 * Copyright (C) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MARKER_SESSION_H
#define MARKER_SESSION_H

#include <gio/gio.h>
#include "marker-project.h"

G_BEGIN_DECLS

typedef struct
{
  GPtrArray *projects;      /* UTF-8 URIs */
  GPtrArray *documents;     /* UTF-8 URIs */
  guint active_project;
  char *active_document;
  gboolean has_navigation;
  gboolean pages_visible;
  gboolean outline_visible;
  gboolean files_mode;
} MarkerSessionWindow;

MarkerSessionWindow *marker_session_window_new          (void);
void                 marker_session_window_free         (MarkerSessionWindow *window);
GPtrArray           *marker_session_load                (GError             **error);
gboolean             marker_session_save                (GPtrArray           *windows,
                                                         GError             **error);
void                 marker_session_restore_project     (MarkerProject       *project);
gboolean             marker_session_store_project       (MarkerProject       *project,
                                                         GError             **error);
void                 marker_session_clear               (void);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MarkerSessionWindow, marker_session_window_free)

G_END_DECLS

#endif
