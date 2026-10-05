/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-notebook-rail.h"
#include "marker-notebook-icon.h"
#include <glib/gi18n.h>

struct _MarkerNotebookRail
{
  GtkBox parent_instance;
  GtkPopover *context_menu;
  MarkerProject *context_project;
};
G_DEFINE_TYPE (MarkerNotebookRail, marker_notebook_rail, GTK_TYPE_BOX)

static guint action_signal;

static void
context_action_cb (GSimpleAction *action, GVariant *parameter, MarkerNotebookRail *self)
{
  if (self->context_project != NULL)
    g_signal_emit (self, action_signal, 0, g_action_get_name (G_ACTION (action)), self->context_project);
}

static void
context_closed_cb (GtkPopover *popover, MarkerNotebookRail *self)
{
  g_clear_object (&self->context_project);
}

static void
show_context (GtkListItem *item, double x, double y)
{
  GtkWidget *child = gtk_list_item_get_child (item);
  MarkerNotebookRail *self = MARKER_NOTEBOOK_RAIL (gtk_widget_get_ancestor (child, MARKER_TYPE_NOTEBOOK_RAIL));
  MarkerProject *project = gtk_list_item_get_item (item);
  graphene_point_t local = GRAPHENE_POINT_INIT (x, y);
  graphene_point_t point;
  if (project == NULL || self == NULL || !gtk_widget_compute_point (child, GTK_WIDGET (self), &local, &point))
    return;
  g_set_object (&self->context_project, project);
  GdkRectangle rectangle = { point.x, point.y, 1, 1 };
  gtk_popover_set_pointing_to (self->context_menu, &rectangle);
  gtk_popover_popup (self->context_menu);
}

static void
pressed_cb (GtkGestureClick *gesture, int n_press, double x, double y, GtkListItem *item)
{
  show_context (item, x, y);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
long_pressed_cb (GtkGestureLongPress *gesture, double x, double y, GtkListItem *item)
{
  show_context (item, x, y);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
selected_cb (GtkListItem *item, GParamSpec *pspec, MarkerNotebookIcon *icon)
{
  marker_notebook_icon_set_selected (icon, gtk_list_item_get_selected (item));
}

static void
setup_cb (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
  GtkWidget *icon = marker_notebook_icon_new (NULL);
  GtkGesture *click = gtk_gesture_click_new ();
  GtkGesture *press = gtk_gesture_long_press_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (click), GDK_BUTTON_SECONDARY);
  g_signal_connect_object (click, "pressed", G_CALLBACK (pressed_cb), item, 0);
  g_signal_connect_object (press, "pressed", G_CALLBACK (long_pressed_cb), item, 0);
  gtk_widget_add_controller (icon, GTK_EVENT_CONTROLLER (click));
  gtk_widget_add_controller (icon, GTK_EVENT_CONTROLLER (press));
  gtk_list_item_set_child (item, icon);
  g_signal_connect_object (item, "notify::selected", G_CALLBACK (selected_cb), icon, 0);
}

static void
bind_cb (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
  MarkerNotebookIcon *icon = MARKER_NOTEBOOK_ICON (gtk_list_item_get_child (item));
  marker_notebook_icon_set_project (icon, gtk_list_item_get_item (item));
  selected_cb (item, NULL, icon);
}

static void
unbind_cb (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
  marker_notebook_icon_set_project (MARKER_NOTEBOOK_ICON (gtk_list_item_get_child (item)), NULL);
}

static void
marker_notebook_rail_dispose (GObject *object)
{
  MarkerNotebookRail *self = MARKER_NOTEBOOK_RAIL (object);
  g_clear_object (&self->context_project);
  if (self->context_menu != NULL)
    {
      gtk_widget_unparent (GTK_WIDGET (self->context_menu));
      self->context_menu = NULL;
    }
  G_OBJECT_CLASS (marker_notebook_rail_parent_class)->dispose (object);
}

static void
marker_notebook_rail_class_init (MarkerNotebookRailClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = marker_notebook_rail_dispose;
  action_signal = g_signal_new ("notebook-action", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                                0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_STRING, MARKER_TYPE_PROJECT);
}

static void
marker_notebook_rail_init (MarkerNotebookRail *self)
{
  const struct { const char *action; const char *label; } entries[] = {
    { "edit", _("Edit…") }, { "reveal", _("Reveal in Files") },
    { "styles", _("Edit Notebook Styles") },
    { "move-up", _("Move Up") }, { "move-down", _("Move Down") },
    { "locate", _("Locate Folder…") }, { "remove", _("Remove") }
  };
  g_autoptr (GMenu) menu = g_menu_new ();
  g_autoptr (GSimpleActionGroup) group = g_simple_action_group_new ();
  for (guint i = 0; i < G_N_ELEMENTS (entries); i++)
    {
      g_autoptr (GSimpleAction) action = g_simple_action_new (entries[i].action, NULL);
      g_autofree char *name = g_strdup_printf ("notebook.%s", entries[i].action);
      g_signal_connect_object (action, "activate", G_CALLBACK (context_action_cb), self, 0);
      g_action_map_add_action (G_ACTION_MAP (group), G_ACTION (action));
      g_menu_append (menu, entries[i].label, name);
    }
  gtk_widget_insert_action_group (GTK_WIDGET (self), "notebook", G_ACTION_GROUP (group));
  self->context_menu = GTK_POPOVER (gtk_popover_menu_new_from_model (G_MENU_MODEL (menu)));
  gtk_widget_set_parent (GTK_WIDGET (self->context_menu), GTK_WIDGET (self));
  gtk_popover_set_has_arrow (self->context_menu, FALSE);
  g_signal_connect (self->context_menu, "closed", G_CALLBACK (context_closed_cb), self);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  gtk_widget_add_css_class (GTK_WIDGET (self), "marker-project-rail");
  GtkWidget *header = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *app_menu = gtk_menu_button_new ();
  g_autoptr (GMenu) app = g_menu_new ();
  g_autoptr (GMenu) open = g_menu_new ();
  g_menu_append (open, _("Open File…"), "win.open-file");
  g_menu_append (open, _("Open Notebook…"), "win.open-folder");
  g_menu_append_submenu (app, _("Open…"), G_MENU_MODEL (open));
  g_menu_append (app, _("New Window"), "app.new");
  g_menu_append (app, _("Preferences"), "win.preferences");
  g_menu_append (app, _("Keyboard Shortcuts"), "app.shortcuts");
  g_menu_append (app, _("Help"), "app.help");
  g_menu_append (app, _("About Marker"), "app.about");
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (app_menu), "open-menu-symbolic");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (app_menu), G_MENU_MODEL (app));
  gtk_widget_set_tooltip_text (app_menu, _("Main Menu"));
  gtk_widget_set_hexpand (app_menu, TRUE);
  gtk_widget_add_css_class (header, "marker-rail-header");
  gtk_box_append (GTK_BOX (header), app_menu);
  gtk_box_append (GTK_BOX (self), header);
}

GtkWidget *
marker_notebook_rail_new (GtkSingleSelection *selection)
{
  MarkerNotebookRail *self = g_object_new (MARKER_TYPE_NOTEBOOK_RAIL, NULL);
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
  GtkWidget *view;
  GtkWidget *scroller = gtk_scrolled_window_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_cb), NULL);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_cb), NULL);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cb), NULL);
  view = gtk_list_view_new (GTK_SELECTION_MODEL (g_object_ref (selection)), factory);
  gtk_list_view_set_single_click_activate (GTK_LIST_VIEW (view), TRUE);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), view);
  gtk_widget_set_vexpand (scroller, TRUE);
  gtk_box_append (GTK_BOX (self), scroller);
  GtkWidget *add = gtk_menu_button_new ();
  g_autoptr (GMenu) menu = g_menu_new ();
  g_menu_append (menu, _("New Notebook…"), "win.new-notebook");
  g_menu_append (menu, _("Open Notebook…"), "win.open-folder");
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (add), "list-add-symbolic");
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (add), G_MENU_MODEL (menu));
  gtk_widget_set_tooltip_text (add, _("Add or Open Notebook"));
  gtk_box_append (GTK_BOX (self), add);
  return GTK_WIDGET (self);
}
