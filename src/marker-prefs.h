/*
 * marker-prefs.h
 *
 * Copyright (C) 2017 - 2018 Fabio Colacio
 *
 * Marker is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * Marker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with Marker; see the file LICENSE.md. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __MARKER_PREFS_H__
#define __MARKER_PREFS_H__

#include "marker-editor.h"

typedef struct {
  GSettings *editor_settings;
  GSettings *preview_settings;
  GSettings *window_settings;
} MarkerPrefs;

extern MarkerPrefs prefs;

gboolean             marker_prefs_get_use_dark_theme             (void);
void                 marker_prefs_set_use_dark_theme             (gboolean            state);
gboolean             marker_prefs_get_follow_system_theme        (void);
gchar               *marker_prefs_get_editor_font                (void);
guint                marker_prefs_get_window_width               (void);
void                 marker_prefs_set_window_width               (guint               width);
guint                marker_prefs_get_window_height              (void);
void                 marker_prefs_set_window_height              (guint               height);
guint                marker_prefs_get_editor_pane_width          (void);
void                 marker_prefs_set_editor_pane_width          (guint               width);
gboolean             marker_prefs_get_show_sidebar               (void);
char                *marker_prefs_get_syntax_theme               (void);
gboolean             marker_prefs_get_replace_tabs               (void);
guint                marker_prefs_get_tab_width                  (void);
gboolean             marker_prefs_get_auto_indent                (void);
gboolean             marker_prefs_get_spell_check                (void);
gchar               *marker_prefs_get_spell_check_language       (void);
gboolean             marker_prefs_get_show_line_numbers          (void);
gboolean             marker_prefs_get_highlight_current_line     (void);
gboolean             marker_prefs_get_wrap_text                  (void);
gboolean             marker_prefs_get_show_right_margin          (void);
guint                marker_prefs_get_right_margin_position      (void);
char                *marker_prefs_get_css_theme                  (void);
void                 marker_prefs_set_css_theme                  (const char         *theme);
gboolean             marker_prefs_get_use_css_theme              (void);
void                 marker_prefs_set_use_css_theme              (gboolean            state);
char                *marker_prefs_get_highlight_theme            (void);
gboolean             marker_prefs_get_use_mathjs                 (void);
gdouble              marker_prefs_get_zoom_level                 (void);
void                 marker_prefs_set_zoom_level                 (gdouble             val);
gboolean             marker_prefs_get_use_highlight              (void);
gboolean             marker_prefs_get_use_mermaid                (void);
gboolean             marker_prefs_get_use_gnuplot                (void);
gchar               *marker_prefs_get_preview_font               (const gchar        *role);
gboolean             marker_prefs_get_use_charter                (void);
MarkerViewMode       marker_prefs_get_default_view_mode          (void);
void                 marker_prefs_set_default_view_mode          (MarkerViewMode      view_mode);
MarkerMathBackEnd    marker_prefs_get_math_backend               (void);
void                 marker_prefs_set_math_backend               (MarkerMathBackEnd   backend);
void                 marker_prefs_load                           (void);
void                 marker_prefs_show_window                    (void);

#endif
