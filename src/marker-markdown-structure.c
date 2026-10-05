/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-markdown-structure.h"
#include <string.h>

static void
heading_free (MarkerHeading *heading)
{
  g_free (heading->title);
  g_free (heading);
}

void
marker_markdown_structure_free (MarkerMarkdownStructure *structure)
{
  if (structure == NULL)
    return;
  g_ptr_array_unref (structure->headings);
  g_array_unref (structure->blocks);
  g_free (structure);
}

static void
add_heading (MarkerMarkdownStructure *structure, guint level, guint line, guint start,
             guint end, guint title_start, const char *title, const char *title_end, gboolean setext)
{
  MarkerHeading *heading = g_new0 (MarkerHeading, 1);
  heading->level = level;
  heading->line = line;
  heading->start = start;
  heading->end = end;
  heading->title_start = title_start;
  heading->title_end = title_start + g_utf8_pointer_to_offset (title, title_end);
  heading->title = g_strndup (title, title_end - title);
  heading->setext = setext;
  g_ptr_array_add (structure->headings, heading);
}

static gboolean
is_science (const char *start, const char *end)
{
  while (start < end && g_ascii_isspace (*start))
    start++;
  const char *word_end = start;
  while (word_end < end && !g_ascii_isspace (*word_end))
    word_end++;
  g_autofree char *word = g_ascii_strdown (start, word_end - start);
  return g_str_equal (word, "mermaid") || g_str_equal (word, "gnuplot") ||
         g_str_equal (word, "math") || g_str_equal (word, "tex") || g_str_equal (word, "latex");
}

MarkerMarkdownStructure *
marker_markdown_structure_parse (const char *markdown)
{
  MarkerMarkdownStructure *structure = g_new0 (MarkerMarkdownStructure, 1);
  structure->headings = g_ptr_array_new_with_free_func ((GDestroyNotify) heading_free);
  structure->blocks = g_array_new (FALSE, FALSE, sizeof (MarkerBlock));
  g_return_val_if_fail (markdown != NULL && g_utf8_validate (markdown, -1, NULL), structure);
  const char *line = markdown;
  const char *previous_title = NULL;
  const char *previous_end = NULL;
  guint previous_start = 0, previous_title_start = 0;
  guint offset = 0, number = 0, fence_length = 0;
  char fence = '\0';
  gboolean metadata = FALSE;
  MarkerBlock block = { 0 };

  while (*line != '\0')
    {
      const char *newline = strchr (line, '\n');
      const char *end = newline != NULL ? newline : line + strlen (line);
      guint line_end = offset + g_utf8_pointer_to_offset (line, end);
      const char *content = line;
      while (content < end && *content == ' ')
        content++;
      const char *trim_end = end;
      while (trim_end > content && g_ascii_isspace (trim_end[-1]))
        trim_end--;
      gboolean indented = content - line >= 4 || (content < end && *content == '\t');
      guint count = 0;
      if (!indented && content < end && (*content == '`' || *content == '~'))
        while (content + count < end && content[count] == *content)
          count++;

      if (number == 0 && trim_end - content == 3 && strncmp (content, "---", 3) == 0)
        {
          metadata = TRUE;
          block = (MarkerBlock) { .kind = MARKER_BLOCK_METADATA, .start = offset, .opening_end = line_end, .content_start = offset };
          previous_title = NULL;
        }
      else if (metadata)
        {
          if (trim_end - content == 3 && (strncmp (content, "---", 3) == 0 || strncmp (content, "...", 3) == 0))
            {
              block.end = line_end;
              block.content_end = line_end;
              g_array_append_val (structure->blocks, block);
              metadata = FALSE;
            }
        }
      else if (fence != '\0')
        {
          if (count >= fence_length && *content == fence && content + count == trim_end)
            {
              block.content_end = offset;
              block.closing_start = offset;
              block.end = line_end;
              block.closed = TRUE;
              g_array_append_val (structure->blocks, block);
              fence = '\0';
            }
        }
      else if (count >= 3)
        {
          fence = *content;
          fence_length = count;
          block = (MarkerBlock) { .kind = is_science (content + count, end) ? MARKER_BLOCK_SCIENCE : MARKER_BLOCK_CODE,
            .start = offset, .opening_end = line_end, .content_start = line_end + (newline != NULL) };
          previous_title = NULL;
        }
      else if (indented)
        {
          MarkerBlock code = { .kind = MARKER_BLOCK_CODE, .start = offset, .end = line_end,
            .content_start = offset, .content_end = line_end };
          g_array_append_val (structure->blocks, code);
          previous_title = NULL;
        }
      else
        {
          guint level = 0;
          while (content + level < end && content[level] == '#')
            level++;
          gboolean underline = content < trim_end && (*content == '=' || *content == '-');
          for (const char *p = content; underline && p < trim_end; p++)
            underline = *p == *content;
          if (level > 0 && level <= 6 && (content + level == end || g_ascii_isspace (content[level])))
            {
              const char *title = content + level;
              while (title < end && g_ascii_isspace (*title))
                title++;
              const char *title_end = trim_end;
              const char *closing = title_end;
              while (closing > title && closing[-1] == '#')
                closing--;
              if (closing > title && closing < title_end && g_ascii_isspace (closing[-1]))
                {
                  title_end = closing;
                  while (title_end > title && g_ascii_isspace (title_end[-1]))
                    title_end--;
                }
              add_heading (structure, level, number, offset, line_end,
                            offset + g_utf8_pointer_to_offset (line, title), title, title_end, FALSE);
              previous_title = NULL;
            }
          else if (underline && previous_title != NULL)
            {
              add_heading (structure, *content == '=' ? 1 : 2, number - 1, previous_start,
                            line_end, previous_title_start, previous_title, previous_end, TRUE);
              previous_title = NULL;
            }
          else
            {
              previous_title = content < trim_end && *content != '>' && !underline ? content : NULL;
              if (content + 1 < end && strchr ("-*+", *content) != NULL && g_ascii_isspace (content[1]))
                previous_title = NULL;
              const char *digits = content;
              while (digits < end && g_ascii_isdigit (*digits))
                digits++;
              if (digits > content && digits + 1 < end && strchr (".)", *digits) != NULL &&
                  g_ascii_isspace (digits[1]))
                previous_title = NULL;
              previous_end = trim_end;
              previous_start = offset;
              previous_title_start = offset + g_utf8_pointer_to_offset (line, content);
            }
        }
      offset = line_end + (newline != NULL);
      number++;
      if (newline == NULL)
        break;
      line = newline + 1;
    }
  if (fence != '\0' || metadata)
    {
      block.end = offset;
      block.content_start = block.start;
      block.content_end = offset;
      g_array_append_val (structure->blocks, block);
    }
  return structure;
}

const MarkerHeading *
marker_markdown_structure_heading_at_line (const MarkerMarkdownStructure *structure, guint line)
{
  for (guint i = 0; i < structure->headings->len; i++)
    {
      const MarkerHeading *heading = g_ptr_array_index (structure->headings, i);
      if (heading->line == line || (heading->setext && heading->line + 1 == line))
        return heading;
    }
  return NULL;
}

char *
marker_markdown_structure_cursor_source (const char *markdown, guint cursor)
{
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  guint offset = MIN (cursor, g_utf8_strlen (markdown, -1));
  const char *point = g_utf8_offset_to_pointer (markdown, offset);
  while (point > markdown && point[-1] != '\n')
    point--;
  offset = g_utf8_pointer_to_offset (markdown, point);
  for (guint i = 0; i < structure->blocks->len; i++)
    {
      MarkerBlock *block = &g_array_index (structure->blocks, MarkerBlock, i);
      if (offset >= block->start && offset <= block->end)
        offset = block->kind == MARKER_BLOCK_METADATA ? MIN (block->end + 1, g_utf8_strlen (markdown, -1)) : block->start;
    }
  point = g_utf8_offset_to_pointer (markdown, offset);
  g_autofree char *before = g_strndup (markdown, point - markdown);
  return g_strconcat (before, "<div id=\"cursor_pos\"></div>\n\n", point, NULL);
}

static gint
compare_rich_ranges (gconstpointer a, gconstpointer b)
{
  const MarkerRichRange *left = a, *right = b;
  return left->start < right->start ? -1 : left->start > right->start;
}

GArray *
marker_markdown_structure_rich_ranges (const char *markdown,
                                     const MarkerMarkdownStructure *structure)
{
  GArray *ranges = g_array_new (FALSE, FALSE, sizeof (MarkerRichRange));
  /* Detect content, not HTML structure. Actual rendering remains SciDown's job.
   * Code spans are the first alternative so dollar signs/images in code stay raw. */
  g_autoptr (GRegex) rich = g_regex_new (
    "(`+)[^`]*\\1|(?i:<img\\b[^>]*>)|!\\[[^\\]\\n]*\\](?:\\([^\\n]+\\)|\\[[^\\]\\n]*\\])|"
    "(?<![\\\\$])\\$\\$[\\s\\S]+?\\$\\$|(?<![\\\\$])\\$[^$\\n]+\\$|"
    "\\\\\\([\\s\\S]+?\\\\\\)|\\\\\\[[\\s\\S]+?\\\\\\]",
    G_REGEX_OPTIMIZE, 0, NULL);
  g_autoptr (GRegex) separator = g_regex_new (
    "^[^\\n]*\\|[^\\n]*\\n[ \\t]*\\|?[ \\t]*:?-{3,}:?[ \\t]*(?:\\|[ \\t]*:?-{3,}:?[ \\t]*)*\\|?[ \\t]*$",
    G_REGEX_MULTILINE | G_REGEX_OPTIMIZE, 0, NULL);

  for (guint i = 0; i < structure->blocks->len; i++)
    {
      const MarkerBlock *block = &g_array_index (structure->blocks, MarkerBlock, i);
      if (block->kind == MARKER_BLOCK_SCIENCE && block->closed)
        {
          MarkerRichRange range = { block->start, block->end };
          g_array_append_val (ranges, range);
        }
    }

  /* Paragraph boundaries preserve inline image/math placement and table rows.
   * Protected code/frontmatter and list/quote nesting are left to source editing. */
  const char *line = markdown, *paragraph = NULL;
  guint paragraph_start = 0, offset = 0;
  gboolean nested = FALSE, display_math = FALSE;
  guint block_index = 0;
  while (TRUE)
    {
      const char *end = strchr (line, '\n');
      if (end == NULL)
        end = line + strlen (line);
      const char *content = line;
      while (content < end && g_ascii_isspace (*content))
        content++;
      gboolean protected = FALSE;
      while (block_index < structure->blocks->len &&
             offset > g_array_index (structure->blocks, MarkerBlock, block_index).end)
        block_index++;
      if (block_index < structure->blocks->len)
        {
          const MarkerBlock *block = &g_array_index (structure->blocks, MarkerBlock, block_index);
          protected = offset >= block->start && offset <= block->end;
        }
      gboolean boundary = (content == end && !display_math) || protected || *line == '\0';
      if (paragraph != NULL && boundary)
        {
          g_autofree char *text = g_strndup (paragraph, line - paragraph);
          g_autoptr (GMatchInfo) match = NULL;
          gboolean render = g_regex_match (separator, text, 0, NULL);
          g_regex_match (rich, text, 0, &match);
          while (!render && g_match_info_matches (match))
            {
              int start;
              g_match_info_fetch_pos (match, 0, &start, NULL);
              render = text[start] != '`';
              g_match_info_next (match, NULL);
            }
          if (render && !nested)
            {
              guint stop = offset;
              while (stop > paragraph_start && g_utf8_offset_to_pointer (markdown, stop)[-1] == '\n')
                stop--;
              MarkerRichRange range = { paragraph_start, stop };
              g_array_append_val (ranges, range);
            }
          paragraph = NULL;
          display_math = FALSE;
        }
      if (!boundary)
        {
          if (paragraph == NULL)
            {
              paragraph = line;
              paragraph_start = offset;
              nested = FALSE;
            }
          nested |= *content == '>' || (content + 1 < end && strchr ("-*+", *content) != NULL && g_ascii_isspace (content[1]));
          const char *digits = content;
          while (digits < end && g_ascii_isdigit (*digits)) digits++;
          nested |= digits > content && digits + 1 < end && strchr (".)", *digits) != NULL && g_ascii_isspace (digits[1]);
          gboolean code = FALSE;
          for (const char *p = line; p < end; p++)
            {
              if (*p == '`') code = !code;
              if (!code && *p == '$' && p + 1 < end && p[1] == '$' && (p == line || p[-1] != '\\'))
                { display_math = !display_math; p++; }
            }
        }
      if (*end == '\0')
        {
          if (paragraph != NULL)
            {
              /* Process the final paragraph through the same boundary branch. */
              offset += g_utf8_pointer_to_offset (line, end);
              line = end;
              continue;
            }
          break;
        }
      offset += g_utf8_pointer_to_offset (line, end) + 1;
      line = end + 1;
    }
  g_array_sort (ranges, compare_rich_ranges);
  return ranges;
}

char *
marker_markdown_structure_rich_source (const char *markdown, const GArray *ranges,
                                      const char *token)
{
  GString *copy = g_string_new (NULL);
  const char *previous = markdown;
  for (guint i = 0; i < ranges->len; i++)
    {
      const MarkerRichRange *range = &g_array_index (ranges, MarkerRichRange, i);
      const char *start = g_utf8_offset_to_pointer (markdown, range->start);
      const char *end = g_utf8_offset_to_pointer (markdown, range->end);
      g_string_append_len (copy, previous, start - previous);
      g_string_append_printf (copy, "\n\n<div id=\"%s-start-%u\"></div>\n\n", token, i);
      g_string_append_len (copy, start, end - start);
      g_string_append_printf (copy, "\n\n<div id=\"%s-end-%u\"></div>\n\n", token, i);
      previous = end;
    }
  g_string_append (copy, previous);
  return g_string_free (copy, FALSE);
}
