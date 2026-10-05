/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-outline.h"
#include <glib/gi18n.h>

struct _MarkerOutline
{
  GtkBox parent_instance;
  MarkerEditor *editor;
  GtkBox *headings;
  guint refresh_source;
};
G_DEFINE_TYPE (MarkerOutline, marker_outline, GTK_TYPE_BOX)

static void
free_mark (GtkTextMark *mark)
{
  GtkTextBuffer *buffer = gtk_text_mark_get_buffer (mark);
  if (buffer != NULL)
    gtk_text_buffer_delete_mark (buffer, mark);
  g_object_unref (mark);
}

static void
jump (GtkButton *button, MarkerOutline *self)
{
  GtkTextMark *mark = g_object_get_data (G_OBJECT (button), "heading-mark");
  GtkTextBuffer *buffer = gtk_text_mark_get_buffer (mark);
  if (self->editor != NULL && buffer != NULL)
    {
      GtkTextIter iter;
      gtk_text_buffer_get_iter_at_mark (buffer, &iter, mark);
      marker_editor_jump_to_heading (self->editor, gtk_text_iter_get_offset (&iter));
    }
}

static gboolean
rebuild (gpointer data)
{
  MarkerOutline *self = MARKER_OUTLINE (data);
  self->refresh_source = 0;
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (self->headings))) != NULL)
    gtk_box_remove (self->headings, child);
  if (self->editor != NULL)
    {
      MarkerSourceView *source = marker_editor_get_source_view (self->editor);
      const MarkerMarkdownStructure *structure = marker_source_view_get_structure (source);
      GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self->editor));
      for (guint i = 0; i < structure->headings->len; i++)
        {
          const MarkerHeading *heading = g_ptr_array_index (structure->headings, i);
          GtkWidget *button = gtk_button_new ();
          GtkWidget *row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
          g_autofree char *level = g_strdup_printf ("H%u", heading->level);
          GtkWidget *marker = gtk_label_new (level);
          GtkWidget *title = gtk_label_new (*heading->title != '\0' ? heading->title : _("Untitled heading"));
          gtk_widget_add_css_class (marker, "marker-secondary");
          gtk_label_set_xalign (GTK_LABEL (title), 0);
          gtk_label_set_ellipsize (GTK_LABEL (title), PANGO_ELLIPSIZE_END);
          gtk_widget_set_hexpand (title, TRUE);
          gtk_box_append (GTK_BOX (row), marker);
          gtk_box_append (GTK_BOX (row), title);
          gtk_button_set_child (GTK_BUTTON (button), row);
          gtk_widget_set_tooltip_text (button, heading->title);
          gtk_widget_set_margin_start (button, (heading->level - 1) * 12);
          gtk_widget_add_css_class (button, "flat");
          GtkTextIter iter;
          gtk_text_buffer_get_iter_at_offset (buffer, &iter, heading->start);
          GtkTextMark *mark = gtk_text_buffer_create_mark (buffer, NULL, &iter, TRUE);
          g_object_set_data_full (G_OBJECT (button), "heading-mark", g_object_ref (mark), (GDestroyNotify) free_mark);
          g_signal_connect (button, "clicked", G_CALLBACK (jump), self);
          gtk_box_append (self->headings, button);
        }
    }
  if (gtk_widget_get_first_child (GTK_WIDGET (self->headings)) == NULL)
    {
      GtkWidget *empty = gtk_label_new (_("No headings yet"));
      gtk_widget_add_css_class (empty, "marker-secondary");
      gtk_widget_set_margin_top (empty, 24);
      gtk_box_append (self->headings, empty);
    }
  return G_SOURCE_REMOVE;
}

static void
queue_refresh (MarkerSourceView *source, MarkerOutline *self)
{
  g_clear_handle_id (&self->refresh_source, g_source_remove);
  self->refresh_source = g_timeout_add_full (G_PRIORITY_LOW, 180, rebuild, self, NULL);
}

void
marker_outline_set_editor (MarkerOutline *self, MarkerEditor *editor)
{
  if (self->editor == editor)
    return;
  g_clear_handle_id (&self->refresh_source, g_source_remove);
  if (self->editor != NULL)
    g_signal_handlers_disconnect_by_func (marker_editor_get_source_view (self->editor), queue_refresh, self);
  g_set_object (&self->editor, editor);
  if (editor != NULL)
    g_signal_connect_object (marker_editor_get_source_view (editor), "structure-changed", G_CALLBACK (queue_refresh), self, 0);
  rebuild (self);
}

static void
dispose (GObject *object)
{
  MarkerOutline *self = MARKER_OUTLINE (object);
  g_clear_handle_id (&self->refresh_source, g_source_remove);
  if (self->editor != NULL)
    g_signal_handlers_disconnect_by_func (marker_editor_get_source_view (self->editor), queue_refresh, self);
  g_clear_object (&self->editor);
  G_OBJECT_CLASS (marker_outline_parent_class)->dispose (object);
}

static void
marker_outline_class_init (MarkerOutlineClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = dispose;
}

static void
marker_outline_init (MarkerOutline *self)
{
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  GtkWidget *scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  self->headings = GTK_BOX (gtk_box_new (GTK_ORIENTATION_VERTICAL, 2));
  gtk_widget_set_margin_start (GTK_WIDGET (self->headings), 8);
  gtk_widget_set_margin_end (GTK_WIDGET (self->headings), 8);
  gtk_widget_set_vexpand (scroller, TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), GTK_WIDGET (self->headings));
  gtk_box_append (GTK_BOX (self), scroller);
}

MarkerOutline *
marker_outline_new (void)
{
  return g_object_new (MARKER_TYPE_OUTLINE, NULL);
}
