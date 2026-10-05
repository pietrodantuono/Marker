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
  return g_test_run ();
}
