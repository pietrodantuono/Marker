/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-line-gutter.h"

struct _MarkerLineGutter { GtkSourceGutterRendererText parent_instance; };
G_DEFINE_TYPE (MarkerLineGutter, marker_line_gutter, GTK_SOURCE_TYPE_GUTTER_RENDERER_TEXT)

static void
query_data (GtkSourceGutterRenderer *renderer, GtkSourceGutterLines *lines, guint line)
{
  GtkTextIter iter;
  gtk_source_gutter_lines_get_iter_at_line (lines, &iter, line);
  GtkTextTag *hidden = gtk_text_tag_table_lookup (
    gtk_text_buffer_get_tag_table (gtk_text_iter_get_buffer (&iter)), "marker-rich-hidden");
  char label[16] = { 0 };
  if (hidden == NULL || !gtk_text_iter_has_tag (&iter, hidden))
    g_snprintf (label, sizeof label, "%u", line + 1);
  gtk_source_gutter_renderer_text_set_text (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer), label, -1);
}

static void
measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
         int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
  int width = 0;
  if (orientation == GTK_ORIENTATION_HORIZONTAL)
    {
      GtkTextBuffer *buffer = GTK_TEXT_BUFFER (gtk_source_gutter_renderer_get_buffer (GTK_SOURCE_GUTTER_RENDERER (widget)));
      char digits[16];
      g_snprintf (digits, sizeof digits, "%u", buffer != NULL ? MAX (99, gtk_text_buffer_get_line_count (buffer)) : 99);
      gtk_source_gutter_renderer_text_measure (GTK_SOURCE_GUTTER_RENDERER_TEXT (widget), digits, &width, NULL);
      width += 12;
    }
  *minimum = *natural = width;
  *minimum_baseline = *natural_baseline = -1;
}

static gboolean
query_activatable (GtkSourceGutterRenderer *renderer, GtkTextIter *iter, GdkRectangle *area)
{
  return TRUE;
}

static void
activate (GtkSourceGutterRenderer *renderer, GtkTextIter *iter, GdkRectangle *area,
          guint button, GdkModifierType state, gint presses)
{
  if (button == GDK_BUTTON_PRIMARY)
    gtk_text_buffer_place_cursor (gtk_text_iter_get_buffer (iter), iter);
}

static void
marker_line_gutter_class_init (MarkerLineGutterClass *klass)
{
  GtkSourceGutterRendererClass *renderer = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
  renderer->query_data = query_data;
  renderer->query_activatable = query_activatable;
  renderer->activate = activate;
  GTK_WIDGET_CLASS (klass)->measure = measure;
}

static void
marker_line_gutter_init (MarkerLineGutter *self)
{
  g_object_set (self, "xalign", 1.0, "yalign", .5, "xpad", 6,
    "alignment-mode", GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST, NULL);
  gtk_widget_add_css_class (GTK_WIDGET (self), "marker-line-gutter");
}

GtkSourceGutterRenderer *
marker_line_gutter_new (void)
{
  return g_object_new (MARKER_TYPE_LINE_GUTTER, NULL);
}
