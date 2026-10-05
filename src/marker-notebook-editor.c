/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-notebook-editor.h"
#include "marker-notebook-icon.h"
#include "marker-session.h"
#include <glib/gi18n.h>

#define MARKER_TYPE_NOTEBOOK_EDITOR (marker_notebook_editor_get_type ())
G_DECLARE_FINAL_TYPE (MarkerNotebookEditor, marker_notebook_editor, MARKER, NOTEBOOK_EDITOR, AdwDialog)
struct _MarkerNotebookEditor
{
  AdwDialog parent_instance;
  MarkerProject *project;
  MarkerProject *draft;
  GtkWidget *name;
  GtkDropDown *mode;
  GtkWidget *custom;
  GtkMenuButton *symbol;
  GtkColorDialogButton *color;
  GtkWidget *apply;
};
G_DEFINE_TYPE (MarkerNotebookEditor, marker_notebook_editor, ADW_TYPE_DIALOG)

static void
name_changed_cb (GtkEditable *entry, MarkerNotebookEditor *self)
{
  g_autofree char *name = g_strdup (gtk_editable_get_text (entry));
  g_strstrip (name);
  gtk_widget_set_sensitive (self->apply, *name != '\0');
  if (*name != '\0')
    marker_project_set_name (self->draft, name);
}

static void
mode_changed_cb (GtkDropDown *mode, GParamSpec *pspec, MarkerNotebookEditor *self)
{
  guint selected = gtk_drop_down_get_selected (mode);
  marker_project_set_icon_mode (self->draft, selected);
  gtk_widget_set_visible (self->custom, selected == MARKER_PROJECT_ICON_CUSTOM);
  gtk_widget_set_visible (GTK_WIDGET (self->symbol), selected == MARKER_PROJECT_ICON_SYMBOL);
}

static void
custom_changed_cb (GtkEditable *entry, MarkerNotebookEditor *self)
{
  marker_project_set_custom_text (self->draft, gtk_editable_get_text (entry));
}

static void
color_changed_cb (GtkColorDialogButton *button, GParamSpec *pspec, MarkerNotebookEditor *self)
{
  const GdkRGBA *rgba = gtk_color_dialog_button_get_rgba (button);
  g_autofree char *color = g_strdup_printf ("#%02x%02x%02x",
    (guint) (rgba->red * 255 + .5), (guint) (rgba->green * 255 + .5), (guint) (rgba->blue * 255 + .5));
  marker_project_set_color (self->draft, color);
}

static void
symbol_clicked_cb (GtkButton *button, MarkerNotebookEditor *self)
{
  const char *icon = g_object_get_data (G_OBJECT (button), "symbol");
  marker_project_set_icon_name (self->draft, icon);
  gtk_menu_button_set_icon_name (self->symbol, icon);
  gtk_menu_button_popdown (self->symbol);
}

static void
apply_clicked_cb (GtkButton *button, MarkerNotebookEditor *self)
{
  g_autoptr (GError) error = NULL;
  if (!marker_session_store_project (self->draft, &error))
    {
      AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (_("Unable to save notebook appearance"), error->message));
      adw_alert_dialog_add_response (dialog, "close", _("Close"));
      adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
      return;
    }
  marker_project_copy_appearance (self->project, self->draft);
  adw_dialog_close (ADW_DIALOG (self));
}

static void
marker_notebook_editor_dispose (GObject *object)
{
  MarkerNotebookEditor *self = MARKER_NOTEBOOK_EDITOR (object);
  g_clear_object (&self->project);
  g_clear_object (&self->draft);
  G_OBJECT_CLASS (marker_notebook_editor_parent_class)->dispose (object);
}

static void
marker_notebook_editor_class_init (MarkerNotebookEditorClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = marker_notebook_editor_dispose;
}

static void
marker_notebook_editor_init (MarkerNotebookEditor *self)
{
}

void
marker_notebook_editor_present (MarkerProject *project, GtkWidget *parent)
{
  static const char *const symbols[] = {
    "car", "code", "food", "gaming", "heart", "home", "like", "music",
    "nature", "patch", "pin", "plant", "plus", "school", "science",
    "settings", "skull", "sport", "star", "toki", "toki-pona", "travel", "work"
  };
  const char *modes[] = { _("First Characters"), _("Initials"), _("Initials: camelCase"),
                          _("Initials: snake_case"), _("Symbol"), _("Custom Text"), NULL };
  MarkerNotebookEditor *self = g_object_new (MARKER_TYPE_NOTEBOOK_EDITOR, NULL);
  self->project = g_object_ref (project);
  self->draft = marker_project_new (marker_project_get_root (project));
  marker_project_copy_appearance (self->draft, project);
  GtkWidget *toolbar = adw_toolbar_view_new ();
  GtkWidget *bar = adw_header_bar_new ();
  GtkWidget *form = gtk_box_new (GTK_ORIENTATION_VERTICAL, 16);
  GtkWidget *preview = marker_notebook_icon_new (self->draft);
  GtkWidget *controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkColorDialog *color_dialog = gtk_color_dialog_new ();
  GdkRGBA rgba;
  gtk_color_dialog_set_with_alpha (color_dialog, FALSE);
  self->color = GTK_COLOR_DIALOG_BUTTON (gtk_color_dialog_button_new (color_dialog));
  gdk_rgba_parse (&rgba, marker_project_get_color (project));
  gtk_color_dialog_button_set_rgba (self->color, &rgba);
  self->name = gtk_entry_new ();
  self->mode = GTK_DROP_DOWN (gtk_drop_down_new_from_strings (modes));
  self->custom = gtk_entry_new ();
  self->symbol = GTK_MENU_BUTTON (gtk_menu_button_new ());
  self->apply = gtk_button_new_with_label (_("Apply"));
  gtk_widget_add_css_class (preview, "marker-book-preview");
  gtk_widget_set_halign (preview, GTK_ALIGN_CENTER);
  gtk_widget_set_margin_top (preview, 12);
  gtk_widget_set_margin_bottom (preview, 16);
  gtk_widget_add_css_class (controls, "linked");
  gtk_widget_add_css_class (self->apply, "suggested-action");
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->mode), _("Notebook icon mode"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->color), _("Notebook color"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->symbol), _("Choose notebook symbol"));
  gtk_entry_set_placeholder_text (GTK_ENTRY (self->name), _("Notebook Name"));
  gtk_editable_set_text (GTK_EDITABLE (self->name), marker_project_get_name (project));
  gtk_editable_set_text (GTK_EDITABLE (self->custom), marker_project_get_custom_text (project) != NULL ? marker_project_get_custom_text (project) : "");
  gtk_editable_set_width_chars (GTK_EDITABLE (self->custom), 2);
  gtk_widget_set_hexpand (GTK_WIDGET (self->mode), TRUE);
  gtk_drop_down_set_selected (self->mode, marker_project_get_icon_mode (project));
  const char *icon = marker_project_get_icon_name (project);
  if (icon == NULL)
    {
      icon = "icon-science-symbolic";
      marker_project_set_icon_name (self->draft, icon);
    }
  gtk_menu_button_set_icon_name (self->symbol, icon);
  GtkWidget *popover = gtk_popover_new ();
  GtkWidget *grid = gtk_flow_box_new ();
  gtk_flow_box_set_selection_mode (GTK_FLOW_BOX (grid), GTK_SELECTION_NONE);
  gtk_flow_box_set_max_children_per_line (GTK_FLOW_BOX (grid), 4);
  gtk_flow_box_set_min_children_per_line (GTK_FLOW_BOX (grid), 4);
  for (guint i = 0; i < G_N_ELEMENTS (symbols); i++)
    {
      g_autofree char *name = g_strdup_printf ("icon-%s-symbolic", symbols[i]);
      GtkWidget *choice = gtk_button_new_from_icon_name (name);
      gtk_widget_add_css_class (choice, "flat");
      gtk_widget_set_tooltip_text (choice, symbols[i]);
      g_object_set_data_full (G_OBJECT (choice), "symbol", g_strdup (name), g_free);
      g_signal_connect_object (choice, "clicked", G_CALLBACK (symbol_clicked_cb), self, 0);
      gtk_flow_box_insert (GTK_FLOW_BOX (grid), choice, -1);
    }
  gtk_popover_set_child (GTK_POPOVER (popover), grid);
  gtk_menu_button_set_popover (self->symbol, popover);
  gtk_box_append (GTK_BOX (controls), GTK_WIDGET (self->mode));
  gtk_box_append (GTK_BOX (controls), self->custom);
  gtk_box_append (GTK_BOX (controls), GTK_WIDGET (self->symbol));
  gtk_box_append (GTK_BOX (controls), GTK_WIDGET (self->color));
  gtk_box_append (GTK_BOX (form), preview);
  gtk_box_append (GTK_BOX (form), self->name);
  gtk_box_append (GTK_BOX (form), controls);
  gtk_box_append (GTK_BOX (form), self->apply);
  gtk_widget_set_margin_start (form, 18);
  gtk_widget_set_margin_end (form, 18);
  gtk_widget_set_margin_bottom (form, 18);
  adw_header_bar_set_title_widget (ADW_HEADER_BAR (bar), gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  adw_toolbar_view_add_top_bar (ADW_TOOLBAR_VIEW (toolbar), bar);
  adw_toolbar_view_set_content (ADW_TOOLBAR_VIEW (toolbar), form);
  adw_dialog_set_child (ADW_DIALOG (self), toolbar);
  adw_dialog_set_content_width (ADW_DIALOG (self), 320);
  adw_dialog_set_title (ADW_DIALOG (self), _("Edit Notebook"));
  g_signal_connect_object (self->name, "changed", G_CALLBACK (name_changed_cb), self, 0);
  g_signal_connect_object (self->mode, "notify::selected", G_CALLBACK (mode_changed_cb), self, 0);
  g_signal_connect_object (self->custom, "changed", G_CALLBACK (custom_changed_cb), self, 0);
  g_signal_connect_object (self->color, "notify::rgba", G_CALLBACK (color_changed_cb), self, 0);
  g_signal_connect_object (self->apply, "clicked", G_CALLBACK (apply_clicked_cb), self, 0);
  mode_changed_cb (self->mode, NULL, self);
  adw_dialog_present (ADW_DIALOG (self), parent);
}
