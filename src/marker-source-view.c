/* marker-source-view.c
 *
 * Copyright (C) 2017-2026 Marker contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-source-view.h"

#include <libspelling.h>
#include <string.h>

#include "marker-formatted.h"
#include "marker-heading-gutter.h"
#include "marker-line-gutter.h"
#include "marker-prefs.h"

struct _MarkerSourceView
{
  GtkSourceView parent_instance;

  GSettings *settings;
  GtkSourceSearchContext *search_context;
  SpellingTextBufferAdapter *spelling;
  GtkCssProvider *font_provider;
  char *font_class;
  MarkerMarkdownStructure *structure;
  GPtrArray *spans;
  GtkSourceGutterRenderer *heading_gutter;
  GtkSourceGutterRenderer *line_gutter;
  guint format_source;
  guint writing_width;
  gboolean formatted;
  gboolean parse_dirty;
  gboolean style_dirty;
};

G_DEFINE_TYPE (MarkerSourceView, marker_source_view, GTK_SOURCE_TYPE_VIEW)

enum { STRUCTURE_CHANGED, CURSOR_CHANGED, LAYOUT_CHANGED, LAST_SIGNAL };
static guint source_signals[LAST_SIGNAL];

static void
ensure_structure (MarkerSourceView *self)
{
  if (!self->parse_dirty)
    return;

  g_autofree char *text = marker_source_view_get_text (self);
  g_clear_pointer (&self->structure, marker_markdown_structure_free);
  g_clear_pointer (&self->spans, g_ptr_array_unref);
  GtkSourceLanguage *language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (
    gtk_text_view_get_buffer (GTK_TEXT_VIEW (self))));
  self->structure = marker_markdown_structure_parse (
    language != NULL && g_str_equal (gtk_source_language_get_id (language), "markdown") ? text : "");
  self->parse_dirty = FALSE;
  self->style_dirty = TRUE;
  gtk_widget_queue_draw (GTK_WIDGET (self->heading_gutter));
  g_signal_emit (self, source_signals[STRUCTURE_CHANGED], 0);
}

static gboolean
format_idle_cb (gpointer user_data)
{
  MarkerSourceView *self = MARKER_SOURCE_VIEW (user_data);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter cursor;
  GtkTextIter selection_start;
  GtkTextIter selection_end;

  self->format_source = 0;
  ensure_structure (self);
  if (self->formatted)
    {
      if (self->spans == NULL)
        {
          g_autofree char *text = marker_source_view_get_text (self);
          self->spans = marker_formatted_parse (text, self->structure);
        }

      gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
      if (!gtk_text_buffer_get_selection_bounds (buffer, &selection_start, &selection_end))
        selection_start = selection_end = cursor;

      marker_formatted_apply (buffer, self->spans,
                              gtk_text_iter_get_offset (&cursor),
                              gtk_text_iter_get_offset (&selection_start),
                              gtk_text_iter_get_offset (&selection_end),
                              TRUE, self->style_dirty);
      self->style_dirty = FALSE;
    }
  g_signal_emit (self, source_signals[CURSOR_CHANGED], 0);
  return G_SOURCE_REMOVE;
}

static void
queue_format (MarkerSourceView *self)
{
  if (self->format_source == 0)
    self->format_source = g_idle_add_full (G_PRIORITY_LOW,
                                          format_idle_cb,
                                          self, NULL);
}

static void
buffer_changed_cb (GtkTextBuffer    *buffer,
                   MarkerSourceView *self)
{
  self->parse_dirty = TRUE;
  gtk_widget_queue_resize (GTK_WIDGET (self->line_gutter));
  queue_format (self);
}

static void
mark_set_cb (GtkTextBuffer    *buffer,
             GtkTextIter      *location,
             GtkTextMark      *mark,
             MarkerSourceView *self)
{
  if (mark == gtk_text_buffer_get_insert (buffer) ||
      mark == gtk_text_buffer_get_selection_bound (buffer))
    queue_format (self);
}

static void
marker_source_view_size_allocate (GtkWidget *widget,
                                  int        width,
                                  int        height,
                                  int        baseline)
{
  MarkerSourceView *self = MARKER_SOURCE_VIEW (widget);

  if (self->formatted)
    {
      int margin = MAX (18, (width - (int) self->writing_width) / 2);
      GtkSourceGutter *gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT);
      int numbers = MAX (0, gtk_widget_get_width (GTK_WIDGET (gutter)) -
                             gtk_widget_get_width (GTK_WIDGET (self->heading_gutter)));
      gtk_widget_set_size_request (GTK_WIDGET (self->heading_gutter), MAX (34, margin - numbers - 18), -1);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self), 18);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self), margin);
    }
  GTK_WIDGET_CLASS (marker_source_view_parent_class)->size_allocate (widget,
                                                                     width,
                                                                     height,
                                                                     baseline);
  g_signal_emit (self, source_signals[LAYOUT_CHANGED], 0);
}

static void
marker_source_view_dispose (GObject *object)
{
  MarkerSourceView *self = MARKER_SOURCE_VIEW (object);

  g_clear_handle_id (&self->format_source, g_source_remove);
  g_clear_object (&self->settings);
  g_clear_object (&self->search_context);
  g_clear_object (&self->spelling);
  if (self->font_provider != NULL)
    gtk_style_context_remove_provider_for_display (
      gtk_widget_get_display (GTK_WIDGET (self)),
      GTK_STYLE_PROVIDER (self->font_provider));
  g_clear_object (&self->font_provider);
  g_clear_pointer (&self->font_class, g_free);
  g_clear_pointer (&self->structure, marker_markdown_structure_free);
  g_clear_pointer (&self->spans, g_ptr_array_unref);
  G_OBJECT_CLASS (marker_source_view_parent_class)->dispose (object);
}

static void
marker_source_view_class_init (MarkerSourceViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

  object_class->dispose = marker_source_view_dispose;
  widget_class->size_allocate = marker_source_view_size_allocate;
  source_signals[STRUCTURE_CHANGED] = g_signal_new ("structure-changed", G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  source_signals[CURSOR_CHANGED] = g_signal_new ("cursor-changed", G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  source_signals[LAYOUT_CHANGED] = g_signal_new ("layout-changed", G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
sync_line_gutter (MarkerSourceView *self)
{
  gboolean show = gtk_source_view_get_show_line_numbers (GTK_SOURCE_VIEW (self));
  GtkSourceGutter *gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT);
  for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (gutter)); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    if (GTK_SOURCE_IS_GUTTER_RENDERER_TEXT (child) && child != GTK_WIDGET (self->heading_gutter) &&
        child != GTK_WIDGET (self->line_gutter))
      gtk_widget_set_visible (child, show && !self->formatted);
  gtk_widget_set_visible (GTK_WIDGET (self->line_gutter), show && self->formatted);
}

static void
marker_source_view_init (MarkerSourceView *self)
{
  static guint next_font_class;
  GtkSourceBuffer *buffer = gtk_source_buffer_new (NULL);
  GtkSourceSearchSettings *search_settings = gtk_source_search_settings_new ();
  SpellingChecker *checker = spelling_checker_get_default ();

  self->settings = g_settings_new ("com.github.fabiocolacio.marker.preferences.editor");
  self->writing_width = g_settings_get_uint (self->settings, "writing-width");
  self->font_provider = gtk_css_provider_new ();
  self->font_class = g_strdup_printf ("marker-source-font-%u", next_font_class++);

  gtk_text_view_set_buffer (GTK_TEXT_VIEW (self), GTK_TEXT_BUFFER (buffer));
  self->parse_dirty = TRUE;
  self->heading_gutter = marker_heading_gutter_new ();
  gtk_source_gutter_insert (gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT),
                            self->heading_gutter, -20);
  gtk_widget_set_visible (GTK_WIDGET (self->heading_gutter), FALSE);
  self->line_gutter = marker_line_gutter_new ();
  gtk_source_gutter_insert (gtk_source_view_get_gutter (GTK_SOURCE_VIEW (self), GTK_TEXT_WINDOW_LEFT),
                            self->line_gutter, -30);
  gtk_widget_set_visible (GTK_WIDGET (self->line_gutter), FALSE);
  g_signal_connect_swapped (self, "notify::show-line-numbers", G_CALLBACK (sync_line_gutter), self);
  gtk_text_view_set_top_margin (GTK_TEXT_VIEW (self), 40);
  gtk_text_view_set_bottom_margin (GTK_TEXT_VIEW (self), 64);
  gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self), 18);
  gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self), 18);
  gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (self), 1);
  gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (self), 1);
  gtk_widget_add_css_class (GTK_WIDGET (self), "marker-source-view");
  gtk_widget_add_css_class (GTK_WIDGET (self), self->font_class);
  gtk_style_context_add_provider_for_display (
    gtk_widget_get_display (GTK_WIDGET (self)),
    GTK_STYLE_PROVIDER (self->font_provider),
    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

  self->search_context = gtk_source_search_context_new (buffer, search_settings);
  self->spelling = spelling_text_buffer_adapter_new (buffer, checker);
  gtk_text_view_set_extra_menu (GTK_TEXT_VIEW (self),
                                spelling_text_buffer_adapter_get_menu_model (self->spelling));
  gtk_widget_insert_action_group (GTK_WIDGET (self), "spelling",
                                  G_ACTION_GROUP (self->spelling));

  g_signal_connect_object (buffer, "changed", G_CALLBACK (buffer_changed_cb), self, 0);
  g_signal_connect_object (buffer, "mark-set", G_CALLBACK (mark_set_cb), self, 0);

  marker_source_view_set_language (self, "markdown");
  marker_source_view_apply_font (self);
  marker_source_view_set_spell_check (self, marker_prefs_get_spell_check ());
  g_autofree char *language = marker_prefs_get_spell_check_language ();
  marker_source_view_set_spell_check_lang (self, language);
  queue_format (self);
  g_object_unref (search_settings);
  g_object_unref (buffer);
}

MarkerSourceView *
marker_source_view_new (void)
{
  return g_object_new (MARKER_TYPE_SOURCE_VIEW,
                       "show-line-numbers", marker_prefs_get_show_line_numbers (),
                       "highlight-current-line", marker_prefs_get_highlight_current_line (),
                       "show-right-margin", marker_prefs_get_show_right_margin (),
                       "right-margin-position", marker_prefs_get_right_margin_position (),
                       "auto-indent", marker_prefs_get_auto_indent (),
                       "insert-spaces-instead-of-tabs", marker_prefs_get_replace_tabs (),
                       "tab-width", marker_prefs_get_tab_width (),
                       NULL);
}

static char *
font_css (const char *font,
          const char *selector)
{
  g_autoptr (PangoFontDescription) description = pango_font_description_from_string (font);
  const char *family = pango_font_description_get_family (description);
  int size = pango_font_description_get_size (description);
  int weight = pango_font_description_get_weight (description);

  if (size <= 0)
    size = 11 * PANGO_SCALE;
  return g_strdup_printf ("%s { font-family: \"%s\"; font-size: %.1fpt; font-weight: %d; }",
                          selector,
                          family != NULL ? family : "monospace",
                          (double) size / PANGO_SCALE,
                          weight);
}

void
marker_source_view_apply_font (MarkerSourceView *self)
{
  g_autofree char *font = NULL;
  g_autofree char *css = NULL;
  g_autofree char *selector = NULL;
  guint line_spacing;

  g_return_if_fail (MARKER_IS_SOURCE_VIEW (self));

  font = self->formatted
           ? g_settings_get_string (self->settings, "prose-font")
           : marker_prefs_get_editor_font ();
  line_spacing = self->formatted
                   ? g_settings_get_uint (self->settings, "line-spacing") : 2;
  gtk_text_view_set_pixels_above_lines (GTK_TEXT_VIEW (self), line_spacing / 2);
  gtk_text_view_set_pixels_below_lines (GTK_TEXT_VIEW (self),
                                        line_spacing - line_spacing / 2);
  selector = g_strdup_printf (".%s", self->font_class);
  css = font_css (font, selector);
  gtk_css_provider_load_from_string (self->font_provider, css);
}

gboolean
marker_source_view_get_modified (MarkerSourceView *self)
{
  return gtk_text_buffer_get_modified (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self)));
}

void
marker_source_view_set_modified (MarkerSourceView *self,
                                 gboolean          modified)
{
  gtk_text_buffer_set_modified (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self)), modified);
}

gchar *
marker_source_view_get_text (MarkerSourceView *self)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter start;
  GtkTextIter end;

  gtk_text_buffer_get_bounds (buffer, &start, &end);
  return gtk_text_buffer_get_text (buffer, &start, &end, FALSE);
}

void
marker_source_view_set_text (MarkerSourceView *self,
                             const char       *text,
                             size_t            size)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter start;

  gtk_text_buffer_set_text (buffer, text != NULL ? text : "", size);
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_place_cursor (buffer, &start);
  gtk_text_buffer_set_modified (buffer, FALSE);
  queue_format (self);
}

void
marker_source_view_set_language (MarkerSourceView *self,
                                 const gchar      *language)
{
  GtkSourceLanguageManager *manager = gtk_source_language_manager_get_default ();
  GtkSourceLanguage *source_language = gtk_source_language_manager_get_language (manager, language);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));

  gtk_source_buffer_set_language (GTK_SOURCE_BUFFER (buffer), source_language);
  gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (buffer), !self->formatted);
  self->parse_dirty = TRUE;
  queue_format (self);
}

void
marker_source_view_set_syntax_theme (MarkerSourceView *self,
                                     const char       *theme)
{
  GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default ();
  GtkSourceStyleScheme *scheme = gtk_source_style_scheme_manager_get_scheme (manager, theme);

  if (self->formatted)
    scheme = gtk_source_style_scheme_manager_get_scheme (manager,
      marker_prefs_get_use_dark_theme () ? "Adwaita-dark" : "Adwaita");
  else if (marker_prefs_get_use_dark_theme ())
    {
      GtkSourceStyleScheme *dark = gtk_source_style_scheme_manager_get_scheme (manager, "oblivion");
      if (dark != NULL)
        scheme = dark;
    }

  gtk_source_buffer_set_style_scheme (GTK_SOURCE_BUFFER (
    gtk_text_view_get_buffer (GTK_TEXT_VIEW (self))), scheme);
}

static void
word_or_selection_bounds (GtkTextBuffer *buffer,
                          GtkTextIter   *start,
                          GtkTextIter   *end,
                          gboolean      *selected)
{
  *selected = gtk_text_buffer_get_selection_bounds (buffer, start, end);
  if (*selected)
    return;

  gtk_text_buffer_get_iter_at_mark (buffer, start, gtk_text_buffer_get_insert (buffer));
  *end = *start;
  if (!gtk_text_iter_starts_word (start))
    gtk_text_iter_backward_word_start (start);
  if (!gtk_text_iter_ends_word (end))
    gtk_text_iter_forward_word_end (end);
}

void
marker_source_view_surround_selection_with (MarkerSourceView *self,
                                            const char       *insertion)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter start;
  GtkTextIter end;
  gboolean selected;
  gsize length = strlen (insertion);
  g_autofree char *contents = NULL;

  word_or_selection_bounds (buffer, &start, &end, &selected);
  contents = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);

  gtk_text_buffer_begin_user_action (buffer);
  if (g_str_has_prefix (contents, insertion) && g_str_has_suffix (contents, insertion) &&
      strlen (contents) >= length * 2)
    {
      GtkTextIter inner_start = start;
      GtkTextIter inner_end = end;
      gtk_text_iter_forward_chars (&inner_start, length);
      gtk_text_iter_backward_chars (&inner_end, length);
      g_autofree char *inner = gtk_text_buffer_get_text (buffer, &inner_start, &inner_end, TRUE);
      gtk_text_buffer_delete (buffer, &start, &end);
      gtk_text_buffer_insert (buffer, &start, inner, -1);
    }
  else
    {
      gtk_text_buffer_insert (buffer, &end, insertion, -1);
      gtk_text_buffer_insert (buffer, &start, insertion, -1);
      if (!selected && *contents == '\0')
        {
          gtk_text_iter_backward_chars (&start, length);
          gtk_text_buffer_place_cursor (buffer, &start);
        }
    }
  gtk_text_buffer_end_user_action (buffer);
}

void
marker_source_view_insert_link (MarkerSourceView *self)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter start;
  GtkTextIter end;

  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    {
      gtk_text_buffer_begin_user_action (buffer);
      gtk_text_buffer_insert (buffer, &end, "]()", -1);
      gtk_text_buffer_insert (buffer, &start, "[", -1);
      gtk_text_buffer_end_user_action (buffer);
    }
  else
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &start, gtk_text_buffer_get_insert (buffer));
      gtk_text_buffer_insert (buffer, &start, "[]()", -1);
      gtk_text_iter_backward_chars (&start, 3);
      gtk_text_buffer_place_cursor (buffer, &start);
    }
}


void
marker_source_view_set_spell_check (MarkerSourceView *self,
                                    gboolean          state)
{
  spelling_text_buffer_adapter_set_enabled (self->spelling, state);
}

void
marker_source_view_set_spell_check_lang (MarkerSourceView *self,
                                         const gchar      *lang)
{
  spelling_text_buffer_adapter_set_language (self->spelling, lang);
}

int
marker_source_view_get_cursor_position (MarkerSourceView *self)
{
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  GtkTextIter cursor;
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
  return gtk_text_iter_get_offset (&cursor);
}

GtkSourceSearchContext *
marker_source_get_search_context (MarkerSourceView *self)
{
  return self->search_context;
}

void
marker_source_view_set_formatted (MarkerSourceView *self,
                                  gboolean          formatted)
{
  GtkTextBuffer *buffer;

  g_return_if_fail (MARKER_IS_SOURCE_VIEW (self));
  if (self->formatted == formatted)
    return;

  self->formatted = formatted;
  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  gtk_source_buffer_set_highlight_syntax (GTK_SOURCE_BUFFER (buffer), !formatted);
  gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (self), marker_prefs_get_show_line_numbers ());
  gtk_source_view_set_show_right_margin (GTK_SOURCE_VIEW (self), !formatted && marker_prefs_get_show_right_margin ());
  gtk_widget_set_visible (GTK_WIDGET (self->heading_gutter), formatted);
  sync_line_gutter (self);
  gtk_widget_set_name (GTK_WIDGET (self), formatted ? "formatted-source" : "source");
  g_autofree char *theme = marker_prefs_get_syntax_theme ();
  marker_source_view_set_syntax_theme (self, theme);
  marker_source_view_apply_font (self);

  if (formatted)
    {
      self->style_dirty = TRUE;
      queue_format (self);
    }
  else
    {
      marker_formatted_clear (buffer);
      gtk_text_view_set_left_margin (GTK_TEXT_VIEW (self), 18);
      gtk_text_view_set_right_margin (GTK_TEXT_VIEW (self), 18);
    }

  gtk_widget_queue_resize (GTK_WIDGET (self));
}


void
marker_source_view_set_writing_width (MarkerSourceView *self,
                                      guint             width)
{
  g_return_if_fail (MARKER_IS_SOURCE_VIEW (self));
  self->writing_width = CLAMP (width, 520, 1400);
  gtk_widget_queue_resize (GTK_WIDGET (self));
}

void
marker_source_view_reapply_formatted (MarkerSourceView *self)
{
  self->style_dirty = TRUE;
  queue_format (self);
}

const MarkerMarkdownStructure *
marker_source_view_get_structure (MarkerSourceView *self)
{
  ensure_structure (self);
  return self->structure;
}

void
marker_source_view_set_heading_level (MarkerSourceView *self, guint line, guint level)
{
  g_return_if_fail (level <= 6);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self));
  const MarkerMarkdownStructure *structure = marker_source_view_get_structure (self);
  const MarkerHeading *heading = marker_markdown_structure_heading_at_line (structure, line);
  GtkTextIter start, end, cursor;
  gtk_text_buffer_get_iter_at_line (buffer, &start, line);
  end = start;
  gtk_text_iter_forward_to_line_end (&end);
  for (guint i = 0; heading == NULL && i < structure->blocks->len; i++)
    {
      const MarkerBlock *block = &g_array_index (structure->blocks, MarkerBlock, i);
      if ((guint) gtk_text_iter_get_offset (&start) >= block->start &&
          (guint) gtk_text_iter_get_offset (&start) <= block->end)
        return;
    }
  g_autofree char *content = NULL;
  guint title_start = gtk_text_iter_get_offset (&start);
  if (heading != NULL)
    {
      content = g_strdup (heading->title);
      title_start = heading->title_start;
      gtk_text_buffer_get_iter_at_offset (buffer, &start, heading->start);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, heading->end);
    }
  else
    content = gtk_text_buffer_get_text (buffer, &start, &end, TRUE);
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
  int relative = CLAMP (gtk_text_iter_get_offset (&cursor) - (int) title_start, 0, g_utf8_strlen (content, -1));
  guint position = gtk_text_iter_get_offset (&start) + (level > 0 ? level + 1 : 0) + relative;
  g_autofree char *replacement = level == 0 ? g_strdup (content) : g_strdup_printf ("%.*s %s", level, "######", content);
  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &start, &end);
  gtk_text_buffer_insert (buffer, &start, replacement, -1);
  gtk_text_buffer_get_iter_at_offset (buffer, &cursor, position);
  gtk_text_buffer_place_cursor (buffer, &cursor);
  gtk_text_buffer_end_user_action (buffer);
}
