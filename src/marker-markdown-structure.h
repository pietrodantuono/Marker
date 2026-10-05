/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef MARKER_MARKDOWN_STRUCTURE_H
#define MARKER_MARKDOWN_STRUCTURE_H
#include <glib.h>
G_BEGIN_DECLS

/* Offsets are Unicode character offsets, matching GtkTextBuffer. */
typedef struct
{
  guint level;
  guint line;
  guint start;
  guint end;
  guint title_start;
  guint title_end;
  gboolean setext;
  char *title;
} MarkerHeading;

typedef enum { MARKER_BLOCK_CODE, MARKER_BLOCK_SCIENCE, MARKER_BLOCK_METADATA } MarkerBlockKind;
typedef struct
{
  MarkerBlockKind kind;
  guint start, end;
  guint content_start, content_end;
  guint opening_end, closing_start;
  gboolean closed;
} MarkerBlock;

typedef struct
{
  GPtrArray *headings; /* MarkerHeading */
  GArray *blocks;     /* MarkerBlock */
} MarkerMarkdownStructure;

MarkerMarkdownStructure *marker_markdown_structure_parse (const char *markdown);
void marker_markdown_structure_free (MarkerMarkdownStructure *structure);
const MarkerHeading *marker_markdown_structure_heading_at_line (const MarkerMarkdownStructure *structure, guint line);
char *marker_markdown_structure_cursor_source (const char *markdown, guint cursor);

/* Whole source regions suitable for non-destructive inline render overlays. */
typedef struct { guint start, end; } MarkerRichRange;
GArray *marker_markdown_structure_rich_ranges (const char *markdown,
                                              const MarkerMarkdownStructure *structure);
char *marker_markdown_structure_rich_source (const char *markdown,
                                            const GArray *ranges,
                                            const char *token);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (MarkerMarkdownStructure, marker_markdown_structure_free)
G_END_DECLS
#endif
