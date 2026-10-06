#include "marker-formatted.h"

static gboolean
has_span (GPtrArray              *spans,
          MarkerMarkdownSpanType  type,
          const char             *markdown,
          const char             *expected)
{
  for (guint i = 0; i < spans->len; i++)
    {
      MarkerMarkdownSpan *span = g_ptr_array_index (spans, i);
      const char *start;
      const char *end;
      g_autofree char *text = NULL;

      if (span->type != type)
        continue;
      start = g_utf8_offset_to_pointer (markdown, span->start);
      end = g_utf8_offset_to_pointer (markdown, span->end);
      text = g_strndup (start, end - start);
      if (g_str_equal (text, expected))
        return TRUE;
    }
  return FALSE;
}

static void
test_parser_spans (void)
{
  const char *markdown =
    "# Héllo **world**\n\n"
    "```mermaid\nA --> B\n```\n\n"
    "[site](https://example.com)\n";
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_autoptr (GPtrArray) spans = marker_formatted_parse (markdown, structure);

  g_assert_true (has_span (spans, MARKER_SPAN_HEADING_1, markdown,
                           "Héllo **world**"));
  g_assert_true (has_span (spans, MARKER_SPAN_STRONG, markdown, "world"));
  g_assert_true (has_span (spans, MARKER_SPAN_SCIENCE_BLOCK, markdown,
                           "A --> B\n"));
  g_assert_true (has_span (spans, MARKER_SPAN_LINK, markdown, "site"));
  g_assert_true (has_span (spans, MARKER_SPAN_MARKER, markdown,
                           "https://example.com"));
}

static void
test_unclosed_fence_stays_visible (void)
{
  const char *markdown = "Before\n```mermaid\nA --> B\n";
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_autoptr (GPtrArray) spans = marker_formatted_parse (markdown, structure);

  g_assert_true (has_span (spans, MARKER_SPAN_SCIENCE_BLOCK, markdown,
                           "```mermaid\nA --> B\n"));
  g_assert_false (has_span (spans, MARKER_SPAN_MARKER, markdown,
                            "```mermaid"));
}

static void
test_code_contents_are_not_reformatted (void)
{
  const char *markdown = "```text\n**literal** and [link](target)\n```\n";
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_autoptr (GPtrArray) spans = marker_formatted_parse (markdown, structure);

  g_assert_true (has_span (spans, MARKER_SPAN_CODE_BLOCK, markdown,
                           "**literal** and [link](target)\n"));
  g_assert_false (has_span (spans, MARKER_SPAN_STRONG, markdown, "literal"));
  g_assert_false (has_span (spans, MARKER_SPAN_LINK, markdown, "link"));
}

static void
test_apply_preserves_source (void)
{
  const char *markdown = "# Title\n\nText with **weight**.\n";
  g_autoptr (GtkTextBuffer) buffer = gtk_text_buffer_new (NULL);
  GtkTextIter start;
  GtkTextIter end;
  g_autofree char *after = NULL;

  gtk_text_buffer_set_text (buffer, markdown, -1);
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_autoptr (GPtrArray) spans = marker_formatted_parse (markdown, structure);
  marker_formatted_apply (buffer, spans, 0, 0, 0, TRUE, TRUE);
  gtk_text_buffer_get_bounds (buffer, &start, &end);
  after = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
  g_assert_cmpstr (after, ==, markdown);
}

static void
test_heading_structure (void)
{
  const char *markdown = "---\n# metadata\n---\n# Héllo ###\n\nTítulo\n======\n"
                         "```gnuplot\n# chart comment\n```\n    # indented\n"
                         "####### invalid\n#not a heading\n###### Six\n\n1. List item\n---\n";
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_assert_cmpuint (structure->headings->len, ==, 3);
  const MarkerHeading *first = g_ptr_array_index (structure->headings, 0);
  g_assert_cmpstr (first->title, ==, "Héllo");
  g_assert_cmpuint (first->line, ==, 3);
  g_assert_cmpuint (first->level, ==, 1);
  g_assert_true (g_str_has_prefix (g_utf8_offset_to_pointer (markdown, first->title_start), "Héllo"));
  const MarkerHeading *setext = g_ptr_array_index (structure->headings, 1);
  g_assert_true (setext->setext);
  g_assert_cmpstr (setext->title, ==, "Título");
  g_assert_true (setext == marker_markdown_structure_heading_at_line (structure, 6));
  g_assert_cmpuint (((MarkerHeading *) g_ptr_array_index (structure->headings, 2))->level, ==, 6);
  g_assert_cmpuint (structure->blocks->len, ==, 3);
}

static void
test_cursor_anchor (void)
{
  const char *markdown = "# Héllo\n\n```gnuplot\nplot 'data.csv'\n```\n";
  guint cursor = g_utf8_pointer_to_offset (markdown, strstr (markdown, "data.csv"));
  g_autofree char *anchored = marker_markdown_structure_cursor_source (markdown, cursor);
  g_assert_nonnull (strstr (anchored, "<div id=\"cursor_pos\"></div>\n\n```gnuplot\nplot 'data.csv'\n```"));
  const char *metadata = "---\ntitle: T\n---\n# Heading\n";
  g_autofree char *metadata_anchor = marker_markdown_structure_cursor_source (metadata, 5);
  g_assert_true (g_str_has_prefix (metadata_anchor, "---\ntitle: T\n---\n<div"));
}

static void
test_inline_code (void)
{
  const char *markdown = "`**literal**` and **bold**";
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_autoptr (GPtrArray) spans = marker_formatted_parse (markdown, structure);
  g_assert_false (has_span (spans, MARKER_SPAN_STRONG, markdown, "literal"));
  g_assert_true (has_span (spans, MARKER_SPAN_STRONG, markdown, "bold"));
}

static void
test_list_structure (void)
{
  const char *markdown = "- Standstill\n1. Operational\n";
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_autoptr (GPtrArray) spans = marker_formatted_parse (markdown, structure);
  g_assert_true (has_span (spans, MARKER_SPAN_LIST, markdown, "- Standstill"));
  g_assert_true (has_span (spans, MARKER_SPAN_LIST, markdown, "1. Operational"));
  g_assert_false (has_span (spans, MARKER_SPAN_MARKER, markdown, "- "));
  g_assert_false (has_span (spans, MARKER_SPAN_MARKER, markdown, "1. "));
}

static void
test_rich_regions (void)
{
  const char *markdown = "# Héllo\n\nBefore ![plot](data/chart.svg) after.\n\n"
    "Inline $x^2$ and `![literal](x)`\n\n"
    "| A | B |\n| --- | ---: |\n| 1 | 2 |\n\n"
    "```gnuplot\nplot sin(x)\n```\n\n"
    "```charter\nplot\n```\n\n"
    "```c\n![hidden](x) $literal$\n```\n\n"
    "- Nested ![asset](x)\n\n"
    "`$literal$`\n\n"
    "Final ![ref][figure]\n\n[figure]: data/chart.svg\n";
  g_autoptr (MarkerMarkdownStructure) structure = marker_markdown_structure_parse (markdown);
  g_autoptr (GArray) ranges = marker_markdown_structure_rich_ranges (markdown, structure);
  g_assert_cmpuint (ranges->len, ==, 6);
  guint previous = 0;
  for (guint i = 0; i < ranges->len; i++)
    {
      MarkerRichRange range = g_array_index (ranges, MarkerRichRange, i);
      g_assert_cmpuint (range.start, >=, previous);
      g_assert_cmpuint (range.end, >, range.start);
      previous = range.end;
    }
  g_autofree char *annotated = marker_markdown_structure_rich_source (markdown, ranges, "test");
  g_assert_nonnull (strstr (annotated, "id=\"test-start-0\"") );
  g_assert_nonnull (strstr (annotated, "Before ![plot](data/chart.svg) after."));
  g_assert_nonnull (strstr (annotated, "[figure]: data/chart.svg"));
  const char *incomplete = "```gnuplot\nplot sin(x)\n";
  g_autoptr (MarkerMarkdownStructure) unfinished = marker_markdown_structure_parse (incomplete);
  g_autoptr (GArray) raw = marker_markdown_structure_rich_ranges (incomplete, unfinished);
  g_assert_cmpuint (raw->len, ==, 0);
  const char *extra = "| One |\n| --- |\n| Value |\n\n$$\n\nx^2\n\n$$\n\n<img src='figure.svg'/>";
  g_autoptr (MarkerMarkdownStructure) extra_structure = marker_markdown_structure_parse (extra);
  g_autoptr (GArray) extra_ranges = marker_markdown_structure_rich_ranges (extra, extra_structure);
  g_assert_cmpuint (extra_ranges->len, ==, 3);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/formatted/parser-spans", test_parser_spans);
  g_test_add_func ("/formatted/unclosed-fence", test_unclosed_fence_stays_visible);
  g_test_add_func ("/formatted/protected-code", test_code_contents_are_not_reformatted);
  g_test_add_func ("/formatted/non-destructive", test_apply_preserves_source);
  g_test_add_func ("/formatted/structure", test_heading_structure);
  g_test_add_func ("/formatted/cursor-anchor", test_cursor_anchor);
  g_test_add_func ("/formatted/inline-code", test_inline_code);
  g_test_add_func ("/formatted/list-structure", test_list_structure);
  g_test_add_func ("/formatted/rich-regions", test_rich_regions);
  return g_test_run ();
}
