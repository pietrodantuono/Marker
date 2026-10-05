/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-notebook-icon.h"

struct _MarkerNotebookIcon
{
  GtkBox parent_instance;
  MarkerProject *project;
  GtkStack *cover;
  GtkLabel *label;
  GtkImage *image;
  GtkWidget *marker;
  GtkCssProvider *provider;
  GdkDisplay *display;
  char *css_class;
};
G_DEFINE_TYPE (MarkerNotebookIcon, marker_notebook_icon, GTK_TYPE_BOX)

static void
appearance_changed_cb (MarkerProject *project, GParamSpec *pspec, MarkerNotebookIcon *self)
{
  const char *color = marker_project_get_color (project);
  g_autofree char *css = g_strdup_printf (
    ".%s .marker-book { background: %s; color: %s; box-shadow: inset 3px 0 shade(%s,.75), inset 0 -2px #a0a5aa; }"
    ".%s.selected { background: alpha(%s,.22); }"
    ".%s .marker-notebook-marker { background: %s; }",
    self->css_class, color, marker_project_get_foreground (project), color,
    self->css_class, color, self->css_class, color);
  gtk_css_provider_load_from_string (self->provider, css);
  gtk_label_set_text (self->label, marker_project_get_text (project));
  gboolean symbol = marker_project_get_icon_mode (project) == MARKER_PROJECT_ICON_SYMBOL &&
                    marker_project_get_icon_name (project) != NULL;
  if (symbol)
    gtk_image_set_from_icon_name (self->image, marker_project_get_icon_name (project));
  gtk_stack_set_visible_child_name (self->cover, symbol ? "symbol" : "text");
  gtk_widget_set_tooltip_text (GTK_WIDGET (self), marker_project_get_name (project));
  gtk_accessible_update_property (GTK_ACCESSIBLE (self), GTK_ACCESSIBLE_PROPERTY_LABEL,
                                  marker_project_get_name (project), -1);
}

void
marker_notebook_icon_set_project (MarkerNotebookIcon *self, MarkerProject *project)
{
  if (self->project != NULL)
    g_signal_handlers_disconnect_by_data (self->project, self);
  g_set_object (&self->project, project);
  if (project != NULL)
    {
      g_signal_connect_object (project, "notify", G_CALLBACK (appearance_changed_cb), self, 0);
      appearance_changed_cb (project, NULL, self);
    }
}

void
marker_notebook_icon_set_selected (MarkerNotebookIcon *self, gboolean selected)
{
  if (selected)
    gtk_widget_add_css_class (GTK_WIDGET (self), "selected");
  else
    gtk_widget_remove_css_class (GTK_WIDGET (self), "selected");
  gtk_widget_set_visible (self->marker, selected);
}

static void
marker_notebook_icon_dispose (GObject *object)
{
  MarkerNotebookIcon *self = MARKER_NOTEBOOK_ICON (object);
  marker_notebook_icon_set_project (self, NULL);
  if (self->provider != NULL)
    gtk_style_context_remove_provider_for_display (self->display, GTK_STYLE_PROVIDER (self->provider));
  g_clear_object (&self->provider);
  g_clear_object (&self->display);
  G_OBJECT_CLASS (marker_notebook_icon_parent_class)->dispose (object);
}

static void
marker_notebook_icon_finalize (GObject *object)
{
  g_free (MARKER_NOTEBOOK_ICON (object)->css_class);
  G_OBJECT_CLASS (marker_notebook_icon_parent_class)->finalize (object);
}

static void
marker_notebook_icon_class_init (MarkerNotebookIconClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = marker_notebook_icon_dispose;
  G_OBJECT_CLASS (klass)->finalize = marker_notebook_icon_finalize;
}

static void
marker_notebook_icon_init (MarkerNotebookIcon *self)
{
  static guint next_class;
  GtkWidget *overlay = gtk_overlay_new ();
  GtkWidget *center = gtk_center_box_new ();
  self->cover = GTK_STACK (gtk_stack_new ());
  self->label = GTK_LABEL (gtk_label_new (NULL));
  self->image = GTK_IMAGE (gtk_image_new ());
  self->marker = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  self->display = g_object_ref (gtk_widget_get_display (GTK_WIDGET (self)));
  self->provider = gtk_css_provider_new ();
  self->css_class = g_strdup_printf ("marker-notebook-%u", next_class++);
  gtk_widget_add_css_class (GTK_WIDGET (self), self->css_class);
  gtk_widget_add_css_class (GTK_WIDGET (self), "marker-notebook-row");
  gtk_widget_add_css_class (GTK_WIDGET (self->cover), "marker-book");
  gtk_widget_add_css_class (self->marker, "marker-notebook-marker");
  gtk_style_context_add_provider_for_display (self->display, GTK_STYLE_PROVIDER (self->provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_label_set_ellipsize (self->label, PANGO_ELLIPSIZE_END);
  gtk_label_set_max_width_chars (self->label, 2);
  gtk_stack_add_named (self->cover, GTK_WIDGET (self->label), "text");
  gtk_stack_add_named (self->cover, GTK_WIDGET (self->image), "symbol");
  gtk_widget_set_valign (GTK_WIDGET (self->cover), GTK_ALIGN_CENTER);
  gtk_center_box_set_center_widget (GTK_CENTER_BOX (center), GTK_WIDGET (self->cover));
  gtk_overlay_set_child (GTK_OVERLAY (overlay), center);
  gtk_widget_set_halign (self->marker, GTK_ALIGN_START);
  gtk_widget_set_valign (self->marker, GTK_ALIGN_CENTER);
  gtk_overlay_add_overlay (GTK_OVERLAY (overlay), self->marker);
  gtk_widget_set_hexpand (overlay, TRUE);
  gtk_box_append (GTK_BOX (self), overlay);
  marker_notebook_icon_set_selected (self, FALSE);
}

GtkWidget *
marker_notebook_icon_new (MarkerProject *project)
{
  MarkerNotebookIcon *self = g_object_new (MARKER_TYPE_NOTEBOOK_ICON, NULL);
  marker_notebook_icon_set_project (self, project);
  return GTK_WIDGET (self);
}
