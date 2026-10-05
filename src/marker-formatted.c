/* marker-formatted.c
 *
 * Non-destructive Markdown presentation for Formatted Markdown mode.
 * The source buffer remains the only document model; this module applies
 * GtkTextTags and never rewrites its contents.
 *
 * Copyright (C) 2026
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-formatted.h"

#include <adwaita.h>
#include <string.h>

static const char *const tag_names[] = {
  "marker-h1", "marker-h2", "marker-h3", "marker-h4", "marker-h5", "marker-h6",
  "marker-emphasis", "marker-strong", "marker-strike", "marker-link", "marker-image",
  "marker-quote", "marker-list", "marker-table", "marker-rule", "marker-inline-code",
  "marker-code-block", "marker-science-block", "marker-delimiter"
};

static guint
char_offset (const char *text,
             gsize       byte_offset)
{
  return g_utf8_pointer_to_offset (text, text + byte_offset);
}

static void
add_span (GPtrArray              *spans,
          MarkerMarkdownSpanType  type,
          const char             *text,
          gsize                   start,
          gsize                   end)
{
  MarkerMarkdownSpan *span;

  if (end <= start)
    return;

  span = g_new (MarkerMarkdownSpan, 1);
  span->type = type;
  span->start = char_offset (text, start);
  span->end = char_offset (text, end);
  g_ptr_array_add (spans, span);
}

void
marker_markdown_span_free (MarkerMarkdownSpan *span)
{
  g_free (span);
}

static void
add_regex_spans (GPtrArray              *spans,
                 const char             *text,
                 const char             *pattern,
                 MarkerMarkdownSpanType  content_type,
                 guint                   content_group,
                 const guint            *marker_groups,
                 guint                   marker_group_count)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GRegex) regex = g_regex_new (pattern,
                                         G_REGEX_MULTILINE | G_REGEX_OPTIMIZE,
                                         0,
                                         &error);
  g_autoptr (GMatchInfo) match = NULL;

  if (regex == NULL || !g_regex_match (regex, text, 0, &match))
    return;

  do
    {
      gint match_start = -1;
      gint match_end = -1;
      gint start = -1;
      gint end = -1;
      gboolean protected = FALSE;

      g_match_info_fetch_pos (match, 0, &match_start, &match_end);
      for (guint i = 0; i < spans->len && !protected; i++)
        {
          MarkerMarkdownSpan *span = g_ptr_array_index (spans, i);
          if ((span->type == MARKER_SPAN_CODE_BLOCK ||
               span->type == MARKER_SPAN_SCIENCE_BLOCK ||
               span->type == MARKER_SPAN_INLINE_CODE) &&
              char_offset (text, match_start) < span->end &&
              char_offset (text, match_end) > span->start)
            protected = TRUE;
        }
      if (protected)
        continue;

      if (g_match_info_fetch_pos (match, content_group, &start, &end) && start >= 0)
        add_span (spans, content_type, text, start, end);

      for (guint i = 0; i < marker_group_count; i++)
        if (g_match_info_fetch_pos (match, marker_groups[i], &start, &end) && start >= 0)
          add_span (spans, MARKER_SPAN_MARKER, text, start, end);
    }
  while (g_match_info_next (match, NULL));
}

static void
add_char_span (GPtrArray *spans, MarkerMarkdownSpanType type, guint start, guint end)
{
  if (end <= start)
    return;
  MarkerMarkdownSpan *span = g_new (MarkerMarkdownSpan, 1);
  *span = (MarkerMarkdownSpan) { type, start, end };
  g_ptr_array_add (spans, span);
}

static void
parse_blocks (GPtrArray *spans, const char *text, const MarkerMarkdownStructure *structure)
{
  for (guint i = 0; i < structure->blocks->len; i++)
    {
      const MarkerBlock *block = &g_array_index (structure->blocks, MarkerBlock, i);
      add_char_span (spans, block->kind == MARKER_BLOCK_SCIENCE ? MARKER_SPAN_SCIENCE_BLOCK : MARKER_SPAN_CODE_BLOCK,
                      block->content_start, block->content_end);
      if (block->closed)
        {
          add_char_span (spans, MARKER_SPAN_MARKER, block->start, block->opening_end);
          add_char_span (spans, MARKER_SPAN_MARKER, block->closing_start, block->end);
        }
    }
  for (guint i = 0; i < structure->headings->len; i++)
    {
      const MarkerHeading *heading = g_ptr_array_index (structure->headings, i);
      add_char_span (spans, MARKER_SPAN_HEADING_1 + heading->level - 1, heading->title_start, heading->title_end);
      add_char_span (spans, MARKER_SPAN_MARKER, heading->start, heading->title_start);
      add_char_span (spans, MARKER_SPAN_MARKER, heading->title_end, heading->end);
    }
  const char *line = text;
  guint line_number = 0;
  while (*line != '\0')
    {
      const char *newline = strchr (line, '\n');
      const char *line_end = newline != NULL ? newline : line + strlen (line);
      gsize start = line - text, end = line_end - text, length = end - start;
      guint offset = char_offset (text, start);
      gboolean protected = marker_markdown_structure_heading_at_line (structure, line_number) != NULL;
      for (guint i = 0; !protected && i < structure->blocks->len; i++)
        {
          const MarkerBlock *block = &g_array_index (structure->blocks, MarkerBlock, i);
          protected = offset >= block->start && offset <= block->end;
        }
      if (!protected && length > 0 && line[0] == '>')
        {
          gsize marker_end = start + 1 + (length > 1 && line[1] == ' ');
          add_span (spans, MARKER_SPAN_BLOCKQUOTE, text, marker_end, end);
          add_span (spans, MARKER_SPAN_MARKER, text, start, marker_end);
        }
      else if (!protected)
        {
          gsize pos = 0;
          while (pos < length && g_ascii_isspace (line[pos]))
            pos++;
          gsize marker_start = pos;
          if (pos + 1 < length && strchr ("-*+", line[pos]) != NULL && g_ascii_isspace (line[pos + 1]))
            pos += 2;
          else if (pos < length && g_ascii_isdigit (line[pos]))
            {
              while (pos < length && g_ascii_isdigit (line[pos]))
                pos++;
              if (pos + 1 < length && strchr (".)", line[pos]) != NULL && g_ascii_isspace (line[pos + 1]))
                pos += 2;
              else
                pos = marker_start;
            }
          if (pos > marker_start)
            {
              /* Keep list punctuation: unlike inline delimiters it conveys
               * structure and has no replacement glyph in the source widget. */
              add_span (spans, MARKER_SPAN_LIST, text, start + marker_start, end);
            }
          else if (length >= 3)
            {
              gboolean rule = TRUE;
              char rule_char = 0;
              guint rule_count = 0;
              for (gsize i = 0; i < length; i++)
                {
                  if (g_ascii_isspace (line[i]))
                    continue;
                  if (rule_char == 0)
                    rule_char = line[i];
                  if (line[i] != rule_char || strchr ("-*_", rule_char) == NULL)
                    rule = FALSE;
                  rule_count++;
                }
              if (rule && rule_count >= 3)
                add_span (spans, MARKER_SPAN_RULE, text, start, end);
              else if (memchr (line, '|', length) != NULL)
                add_span (spans, MARKER_SPAN_TABLE, text, start, end);
            }
        }
      line_number++;
      if (newline == NULL)
        break;
      line = newline + 1;
    }
}

GPtrArray *
marker_formatted_parse (const char *markdown, const MarkerMarkdownStructure *structure)
{
  static const guint pair_markers[] = { 1, 3 };
  static const guint image_markers[] = { 1, 2, 4, 5, 6 };
  g_autoptr (GPtrArray) spans = g_ptr_array_new_with_free_func ((GDestroyNotify) marker_markdown_span_free);

  g_return_val_if_fail (markdown != NULL, g_steal_pointer (&spans));

  parse_blocks (spans, markdown, structure);
  add_regex_spans (spans, markdown, "(`)([^`\\n]+)(`)",
                   MARKER_SPAN_INLINE_CODE, 2, pair_markers, G_N_ELEMENTS (pair_markers));
  add_regex_spans (spans, markdown, "(\\*\\*|__)([^\\n]+?)(\\*\\*|__)",
                   MARKER_SPAN_STRONG, 2, pair_markers, G_N_ELEMENTS (pair_markers));
  add_regex_spans (spans, markdown, "(~~)([^\\n]+?)(~~)",
                   MARKER_SPAN_STRIKE, 2, pair_markers, G_N_ELEMENTS (pair_markers));
  add_regex_spans (spans, markdown, "(^|[^*_])([*_])([^\\n*_]+)([*_])",
                   MARKER_SPAN_EMPHASIS, 3, (const guint[]) { 2, 4 }, 2);
  add_regex_spans (spans, markdown, "(!)(\\[)([^]\\n]*)(\\]\\()([^\\n)]*)(\\))",
                   MARKER_SPAN_IMAGE, 3, image_markers, G_N_ELEMENTS (image_markers));
  add_regex_spans (spans, markdown, "(^|[^!])(\\[)([^]\\n]+)(\\]\\()([^\\n)]+)(\\))",
                   MARKER_SPAN_LINK, 3, (const guint[]) { 2, 4, 5, 6 }, 4);

  return g_steal_pointer (&spans);
}

static GtkTextTag *
ensure_tag (GtkTextBuffer *buffer,
            const char    *name)
{
  GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
  GtkTextTag *tag = gtk_text_tag_table_lookup (table, name);

  if (tag == NULL)
    {
      tag = gtk_text_tag_new (name);
      gtk_text_tag_table_add (table, tag);
      g_object_unref (tag);
    }
  return tag;
}

static GdkRGBA
theme_color (const char *light,
             const char *dark,
             double      alpha)
{
  GdkRGBA color = { 0 };
  gboolean dark_mode = gtk_is_initialized () &&
                       adw_style_manager_get_dark (adw_style_manager_get_default ());

  gdk_rgba_parse (&color, dark_mode ? dark : light);
  color.alpha = alpha;
  return color;
}

void
marker_formatted_prepare_buffer (GtkTextBuffer *buffer)
{
  static const double heading_scales[] = { 1.75, 1.50, 1.30, 1.15, 1.08, 1.0 };
  g_autoptr (GSettings) settings = NULL;
  g_autofree char *code_font = NULL;
  g_autoptr (PangoFontDescription) code_description = NULL;
  GdkRGBA link = theme_color ("#1c71d8", "#62a0ea", 1.0);
  GdkRGBA image = theme_color ("#1a7f4b", "#57e389", 1.0);
  GdkRGBA muted = theme_color ("#5e5c64", "#c0bfbc", 1.0);
  GdkRGBA table_background = theme_color ("#77767b", "#deddda", 0.08);
  GdkRGBA code_background = theme_color ("#77767b", "#deddda", 0.10);
  GdkRGBA science_background = theme_color ("#3584e4", "#62a0ea", 0.14);
  GdkRGBA hidden = { 0, 0, 0, 0 };
  guint line_spacing;
  GtkTextTag *tag;

  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  settings = g_settings_new ("com.github.fabiocolacio.marker.preferences.editor");
  code_font = g_settings_get_string (settings, "code-font");
  code_description = pango_font_description_from_string (code_font);
  line_spacing = g_settings_get_uint (settings, "line-spacing");

  for (guint i = 0; i < 6; i++)
    {
      tag = ensure_tag (buffer, tag_names[i]);
      g_object_set (tag,
                    "weight", PANGO_WEIGHT_BOLD,
                    "scale", heading_scales[i],
                    "pixels-above-lines", (i < 2 ? 14 : 9) + line_spacing / 2,
                    "pixels-below-lines", 5 + line_spacing / 2,
                    NULL);
    }

  g_object_set (ensure_tag (buffer, "marker-emphasis"), "style", PANGO_STYLE_ITALIC, NULL);
  g_object_set (ensure_tag (buffer, "marker-strong"), "weight", PANGO_WEIGHT_BOLD, NULL);
  g_object_set (ensure_tag (buffer, "marker-strike"), "strikethrough", TRUE, NULL);
  g_object_set (ensure_tag (buffer, "marker-link"), "foreground-rgba", &link, "underline", PANGO_UNDERLINE_SINGLE, NULL);
  g_object_set (ensure_tag (buffer, "marker-image"), "foreground-rgba", &image, "weight", PANGO_WEIGHT_MEDIUM, NULL);
  g_object_set (ensure_tag (buffer, "marker-quote"), "style", PANGO_STYLE_ITALIC, "foreground-rgba", &muted, "left-margin", 36, NULL);
  g_object_set (ensure_tag (buffer, "marker-list"), "left-margin", 30, NULL);
  g_object_set (ensure_tag (buffer, "marker-table"), "font-desc", code_description, "background-rgba", &table_background, NULL);
  g_object_set (ensure_tag (buffer, "marker-rule"), "foreground-rgba", &muted, "weight", PANGO_WEIGHT_BOLD, NULL);
  g_object_set (ensure_tag (buffer, "marker-inline-code"), "font-desc", code_description, "background-rgba", &code_background, NULL);
  g_object_set (ensure_tag (buffer, "marker-code-block"), "font-desc", code_description, "background-rgba", &code_background, "left-margin", 30, NULL);
  g_object_set (ensure_tag (buffer, "marker-science-block"), "font-desc", code_description, "background-rgba", &science_background, "left-margin", 30, NULL);
  g_object_set (ensure_tag (buffer, "marker-delimiter"), "foreground-rgba", &muted, NULL);
  /* GtkSourceView's annotation layer cannot safely map lines containing
   * GtkTextTag:invisible ranges while the view is scrolling.  Transparent,
   * near-zero-sized delimiters provide the same presentation without
   * corrupting its visible-line index. */
  g_object_set (ensure_tag (buffer, "marker-delimiter-hidden"),
                "foreground-rgba", &hidden,
                "scale", 0.01,
                "invisible", FALSE,
                NULL);
}

static guint
line_for_offset (GtkTextBuffer *buffer,
                 guint          offset)
{
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
  return gtk_text_iter_get_line (&iter);
}

void
marker_formatted_clear (GtkTextBuffer *buffer)
{
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  GtkTextTagTable *table = gtk_text_buffer_get_tag_table (buffer);
  for (guint i = 0; i < G_N_ELEMENTS (tag_names); i++)
    {
      GtkTextTag *tag = gtk_text_tag_table_lookup (table, tag_names[i]);
      if (tag != NULL)
        gtk_text_buffer_remove_tag (buffer, tag, &start, &end);
    }
  GtkTextTag *hidden = gtk_text_tag_table_lookup (table, "marker-delimiter-hidden");
  if (hidden != NULL)
    gtk_text_buffer_remove_tag (buffer, hidden, &start, &end);
}

void
marker_formatted_apply (GtkTextBuffer *buffer, const GPtrArray *spans,
                        guint cursor_offset, guint selection_start, guint selection_end,
                        gboolean hide_markers, gboolean apply_styles)
{
  GtkTextIter start, end;
  g_return_if_fail (GTK_IS_TEXT_BUFFER (buffer));
  if (apply_styles)
    {
      marker_formatted_prepare_buffer (buffer);
      marker_formatted_clear (buffer);
    }
  else
    {
      gtk_text_buffer_get_bounds (buffer, &start, &end);
      gtk_text_buffer_remove_tag_by_name (buffer, "marker-delimiter-hidden", &start, &end);
    }
  guint cursor_line = line_for_offset (buffer, cursor_offset);
  for (guint i = 0; i < spans->len; i++)
    {
      MarkerMarkdownSpan *span = g_ptr_array_index (spans, i);
      if (!apply_styles && span->type != MARKER_SPAN_MARKER)
        continue;
      gtk_text_buffer_get_iter_at_offset (buffer, &start, span->start);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, span->end);
      if (apply_styles)
        gtk_text_buffer_apply_tag_by_name (buffer, tag_names[span->type], &start, &end);
      gboolean intersects_selection = selection_start != selection_end && span->start < selection_end && span->end > selection_start;
      if (span->type == MARKER_SPAN_MARKER && hide_markers && !intersects_selection &&
          gtk_text_iter_get_line (&start) != (gint) cursor_line)
        gtk_text_buffer_apply_tag_by_name (buffer, "marker-delimiter-hidden", &start, &end);
    }
}
