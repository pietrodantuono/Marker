/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-heading-gutter.h"
#include "marker-source-view.h"
#include <glib/gi18n.h>

struct _MarkerHeadingGutter
{
  GtkSourceGutterRendererText parent_instance;
  GtkPopover *popover;
  GtkTextBuffer *target_buffer;
  GtkTextMark *target;
};
G_DEFINE_TYPE (MarkerHeadingGutter, marker_heading_gutter, GTK_SOURCE_TYPE_GUTTER_RENDERER_TEXT)

static const MarkerHeading *
heading_for_line (GtkSourceGutterRenderer *renderer, guint line)
{
  MarkerSourceView *view = MARKER_SOURCE_VIEW (gtk_source_gutter_renderer_get_view (renderer));
  if (view == NULL)
    return NULL;
  return marker_markdown_structure_heading_at_line (marker_source_view_get_structure (view), line);
}

static void
query_data (GtkSourceGutterRenderer *renderer, GtkSourceGutterLines *lines, guint line)
{
  const MarkerHeading *heading = heading_for_line (renderer, line);
  char label[3] = { 0 };
  if (heading != NULL && heading->line == line)
    g_snprintf (label, sizeof label, "H%u", heading->level);
  gtk_source_gutter_renderer_text_set_text (GTK_SOURCE_GUTTER_RENDERER_TEXT (renderer), label, -1);
}

static gboolean
query_activatable (GtkSourceGutterRenderer *renderer, GtkTextIter *iter, GdkRectangle *area)
{
  return heading_for_line (renderer, gtk_text_iter_get_line (iter)) != NULL;
}

static void
clear_target (MarkerHeadingGutter *self)
{
  if (self->target != NULL)
    gtk_text_buffer_delete_mark (self->target_buffer, self->target);
  g_clear_object (&self->target);
  g_clear_object (&self->target_buffer);
}

static void
choose_heading (GtkButton *button, MarkerHeadingGutter *self)
{
  MarkerSourceView *view = MARKER_SOURCE_VIEW (gtk_source_gutter_renderer_get_view (GTK_SOURCE_GUTTER_RENDERER (self)));
  if (view != NULL && self->target != NULL)
    {
      GtkTextIter iter;
      gtk_text_buffer_get_iter_at_mark (self->target_buffer, &iter, self->target);
      guint level = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "heading-level"));
      marker_source_view_set_heading_level (view, gtk_text_iter_get_line (&iter), level);
      gtk_widget_grab_focus (GTK_WIDGET (view));
    }
  gtk_popover_popdown (self->popover);
}

static void
activate (GtkSourceGutterRenderer *renderer, GtkTextIter *iter, GdkRectangle *area,
          guint button, GdkModifierType state, gint presses)
{
  MarkerHeadingGutter *self = MARKER_HEADING_GUTTER (renderer);
  if (button != GDK_BUTTON_PRIMARY)
    return;
  clear_target (self);
  self->target_buffer = g_object_ref (gtk_text_iter_get_buffer (iter));
  self->target = g_object_ref (gtk_text_buffer_create_mark (self->target_buffer, NULL, iter, TRUE));
  if (self->popover == NULL)
    {
      self->popover = GTK_POPOVER (gtk_popover_new ());
      GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      const char *labels[] = { _("Paragraph"), _("Heading 1"), _("Heading 2"), _("Heading 3"),
                               _("Heading 4"), _("Heading 5"), _("Heading 6") };
      for (guint i = 0; i < G_N_ELEMENTS (labels); i++)
        {
          GtkWidget *choice = gtk_button_new_with_label (labels[i]);
          gtk_widget_add_css_class (choice, "flat");
          g_object_set_data (G_OBJECT (choice), "heading-level", GUINT_TO_POINTER (i));
          g_signal_connect (choice, "clicked", G_CALLBACK (choose_heading), self);
          gtk_box_append (GTK_BOX (box), choice);
        }
      gtk_popover_set_child (self->popover, box);
      gtk_widget_set_parent (GTK_WIDGET (self->popover), GTK_WIDGET (self));
      g_signal_connect_swapped (self->popover, "closed", G_CALLBACK (clear_target), self);
    }
  GdkRectangle point = *area;
  point.x = MAX (0, gtk_widget_get_width (GTK_WIDGET (self)) - 34);
  point.width = 34;
  gtk_popover_set_pointing_to (self->popover, &point);
  gtk_popover_popup (self->popover);
}

static void
dispose (GObject *object)
{
  MarkerHeadingGutter *self = MARKER_HEADING_GUTTER (object);
  clear_target (self);
  if (self->popover != NULL)
    {
      gtk_widget_unparent (GTK_WIDGET (self->popover));
      self->popover = NULL;
    }
  G_OBJECT_CLASS (marker_heading_gutter_parent_class)->dispose (object);
}

static void
marker_heading_gutter_class_init (MarkerHeadingGutterClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = dispose;
  GtkSourceGutterRendererClass *renderer_class = GTK_SOURCE_GUTTER_RENDERER_CLASS (klass);
  renderer_class->query_data = query_data;
  renderer_class->query_activatable = query_activatable;
  renderer_class->activate = activate;
}

static void
marker_heading_gutter_init (MarkerHeadingGutter *self)
{
  gtk_source_gutter_renderer_set_xalign (GTK_SOURCE_GUTTER_RENDERER (self), 1.0);
  gtk_source_gutter_renderer_set_yalign (GTK_SOURCE_GUTTER_RENDERER (self), .5);
  gtk_source_gutter_renderer_set_alignment_mode (GTK_SOURCE_GUTTER_RENDERER (self), GTK_SOURCE_GUTTER_RENDERER_ALIGNMENT_MODE_FIRST);
  gtk_widget_set_size_request (GTK_WIDGET (self), 34, -1);
  gtk_widget_add_css_class (GTK_WIDGET (self), "marker-heading-gutter");
  gtk_widget_set_tooltip_text (GTK_WIDGET (self), _("Change heading level"));
}

GtkSourceGutterRenderer *
marker_heading_gutter_new (void)
{
  return g_object_new (MARKER_TYPE_HEADING_GUTTER, NULL);
}
