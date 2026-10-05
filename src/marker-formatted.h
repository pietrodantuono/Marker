/* marker-formatted.h
 *
 * Copyright (C) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef MARKER_FORMATTED_H
#define MARKER_FORMATTED_H

#include <gtk/gtk.h>
#include "marker-markdown-structure.h"

G_BEGIN_DECLS

typedef enum
{
  MARKER_SPAN_HEADING_1,
  MARKER_SPAN_HEADING_2,
  MARKER_SPAN_HEADING_3,
  MARKER_SPAN_HEADING_4,
  MARKER_SPAN_HEADING_5,
  MARKER_SPAN_HEADING_6,
  MARKER_SPAN_EMPHASIS,
  MARKER_SPAN_STRONG,
  MARKER_SPAN_STRIKE,
  MARKER_SPAN_LINK,
  MARKER_SPAN_IMAGE,
  MARKER_SPAN_BLOCKQUOTE,
  MARKER_SPAN_LIST,
  MARKER_SPAN_TABLE,
  MARKER_SPAN_RULE,
  MARKER_SPAN_INLINE_CODE,
  MARKER_SPAN_CODE_BLOCK,
  MARKER_SPAN_SCIENCE_BLOCK,
  MARKER_SPAN_MARKER
} MarkerMarkdownSpanType;

typedef struct
{
  MarkerMarkdownSpanType type;
  guint start;
  guint end;
} MarkerMarkdownSpan;

GPtrArray *marker_formatted_parse               (const char    *markdown,
                                                 const MarkerMarkdownStructure *structure);
void       marker_markdown_span_free            (MarkerMarkdownSpan *span);
void       marker_formatted_prepare_buffer      (GtkTextBuffer *buffer);
void       marker_formatted_clear               (GtkTextBuffer *buffer);
void       marker_formatted_apply               (GtkTextBuffer *buffer,
                                                 const GPtrArray *spans,
                                                 guint          cursor_offset,
                                                 guint          selection_start,
                                                 guint          selection_end,
                                                 gboolean       hide_markers,
                                                 gboolean       apply_styles);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MarkerMarkdownSpan, marker_markdown_span_free)

G_END_DECLS

#endif
