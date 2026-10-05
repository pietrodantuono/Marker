/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MARKER_PROJECT_H
#define MARKER_PROJECT_H
#include <gio/gio.h>
G_BEGIN_DECLS
#define MARKER_TYPE_PROJECT (marker_project_get_type ())
G_DECLARE_FINAL_TYPE (MarkerProject, marker_project, MARKER, PROJECT, GObject)
typedef enum
{
  MARKER_PROJECT_ICON_FIRST,
  MARKER_PROJECT_ICON_INITIALS,
  MARKER_PROJECT_ICON_CAMEL,
  MARKER_PROJECT_ICON_SNAKE,
  MARKER_PROJECT_ICON_SYMBOL,
  MARKER_PROJECT_ICON_CUSTOM
} MarkerProjectIconMode;
MarkerProject *marker_project_new                 (GFile *root);
GFile         *marker_project_get_root            (MarkerProject *self);
const char    *marker_project_get_uri             (MarkerProject *self);
const char    *marker_project_get_name            (MarkerProject *self);
const char    *marker_project_get_text            (MarkerProject *self);
const char    *marker_project_get_icon_name       (MarkerProject *self);
const char    *marker_project_get_custom_text     (MarkerProject *self);
const char    *marker_project_get_color           (MarkerProject *self);
const char    *marker_project_get_foreground      (MarkerProject *self);
MarkerProjectIconMode marker_project_get_icon_mode (MarkerProject *self);
gboolean       marker_project_get_missing         (MarkerProject *self);
void           marker_project_set_name            (MarkerProject *self, const char *name);
void           marker_project_set_icon_name       (MarkerProject *self, const char *name);
void           marker_project_set_custom_text     (MarkerProject *self, const char *text);
void           marker_project_set_icon_mode       (MarkerProject *self, MarkerProjectIconMode mode);
void           marker_project_set_color           (MarkerProject *self, const char *color);
void           marker_project_copy_appearance     (MarkerProject *self, MarkerProject *source);
void           marker_project_relocate            (MarkerProject *self, GFile *root);
gboolean       marker_project_contains            (MarkerProject *self, GFile *file);
GFile         *marker_project_ensure_stylesheet    (MarkerProject *self, GError **error);
G_END_DECLS
#endif
