#include "marker-notebook-editor.h"
#include "marker-notebook-icon.h"
#include "marker-notebook-rail.h"
#include "marker-session.h"
#include <glib/gstdio.h>

static void
drain_events (void)
{
  while (g_main_context_iteration (NULL, FALSE));
}

static GtkWidget *
find_widget (GtkWidget *parent, GType type, const char *text)
{
  if (g_type_is_a (G_OBJECT_TYPE (parent), type) &&
      (text == NULL || (GTK_IS_BUTTON (parent) && g_strcmp0 (gtk_button_get_label (GTK_BUTTON (parent)), text) == 0)))
    return parent;
  for (GtkWidget *child = gtk_widget_get_first_child (parent); child != NULL; child = gtk_widget_get_next_sibling (child))
    {
      GtkWidget *found = find_widget (child, type, text);
      if (found != NULL)
        return found;
    }
  return NULL;
}

static void
test_editor_apply_cancel (void)
{
  g_autoptr (GFile) root = g_file_new_for_path ("/tmp/widget-notebook");
  g_autoptr (MarkerProject) project = marker_project_new (root);
  GtkWidget *window = adw_window_new ();
  gtk_window_set_default_size (GTK_WINDOW (window), 600, 600);
  adw_window_set_content (ADW_WINDOW (window), marker_notebook_icon_new (project));
  gtk_window_present (GTK_WINDOW (window));
  drain_events ();
  marker_notebook_editor_present (project, window);
  AdwDialog *dialog = adw_window_get_visible_dialog (ADW_WINDOW (window));
  GtkWidget *entry = find_widget (GTK_WIDGET (dialog), GTK_TYPE_ENTRY, NULL);
  g_assert_nonnull (entry);
  gtk_editable_set_text (GTK_EDITABLE (entry), "Draft Name");
  g_assert_cmpstr (marker_project_get_name (project), ==, "widget-notebook");
  adw_dialog_close (dialog);
  drain_events ();
  marker_notebook_editor_present (project, window);
  dialog = adw_window_get_visible_dialog (ADW_WINDOW (window));
  entry = find_widget (GTK_WIDGET (dialog), GTK_TYPE_ENTRY, NULL);
  gtk_editable_set_text (GTK_EDITABLE (entry), "Applied Name");
  GtkWidget *apply = find_widget (GTK_WIDGET (dialog), GTK_TYPE_BUTTON, "Apply");
  g_signal_emit_by_name (apply, "clicked");
  g_assert_cmpstr (marker_project_get_name (project), ==, "Applied Name");
  g_autoptr (MarkerProject) restored = marker_project_new (root);
  marker_session_restore_project (restored);
  g_assert_cmpstr (marker_project_get_name (restored), ==, "Applied Name");
  gtk_window_destroy (GTK_WINDOW (window));
  drain_events ();
}

static void
context_action_cb (MarkerNotebookRail *rail, const char *action, MarkerProject *project, MarkerProject **received)
{
  g_assert_cmpstr (action, ==, "edit");
  *received = project;
}

static void
test_rail_selection_and_lifetime (void)
{
  g_autoptr (GFile) root = g_file_new_for_path ("/tmp/rail-notebook");
  g_autoptr (MarkerProject) project = marker_project_new (root);
  g_autoptr (GListStore) store = g_list_store_new (MARKER_TYPE_PROJECT);
  g_list_store_append (store, project);
  g_autoptr (GtkSingleSelection) selection = gtk_single_selection_new (G_LIST_MODEL (g_object_ref (store)));
  GtkWidget *window = adw_window_new ();
  GtkWidget *rail = marker_notebook_rail_new (selection);
  adw_window_set_content (ADW_WINDOW (window), rail);
  gtk_window_present (GTK_WINDOW (window));
  drain_events ();
  GtkWidget *icon = find_widget (rail, MARKER_TYPE_NOTEBOOK_ICON, NULL);
  g_assert_nonnull (icon);
  g_assert_true (gtk_widget_has_css_class (icon, "selected"));
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_CUSTOM);
  marker_project_set_custom_text (project, "🧪");
  GtkWidget *label = find_widget (icon, GTK_TYPE_LABEL, NULL);
  g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (label)), ==, "🧪");
  MarkerProject *received = NULL;
  g_signal_connect (rail, "notebook-action", G_CALLBACK (context_action_cb), &received);
  g_autoptr (GListModel) controllers = gtk_widget_observe_controllers (icon);
  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++)
    {
      g_autoptr (GObject) controller = g_list_model_get_item (controllers, i);
      if (GTK_IS_GESTURE_LONG_PRESS (controller))
        g_signal_emit_by_name (controller, "pressed", 10., 10.);
    }
  gtk_single_selection_set_can_unselect (selection, TRUE);
  gtk_single_selection_set_autoselect (selection, FALSE);
  gtk_single_selection_set_selected (selection, GTK_INVALID_LIST_POSITION);
  g_assert_false (gtk_widget_has_css_class (icon, "selected"));
  g_assert_true (gtk_widget_activate_action (rail, "notebook.edit", NULL));
  g_assert_true (received == project);
  gtk_window_destroy (GTK_WINDOW (window));
  drain_events ();
  /* A surviving model must not call into destroyed widget/CSS providers. */
  marker_project_set_color (project, "#ac1234");
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  g_autofree char *temporary = g_dir_make_tmp ("marker-widget-state-XXXXXX", NULL);
  g_setenv ("MARKER_STATE_DIR", temporary, TRUE);
  if (!gtk_init_check ())
    return 77;
  adw_init ();
  gtk_icon_theme_add_resource_path (gtk_icon_theme_get_for_display (gdk_display_get_default ()),
                                    "/com/github/fabiocolacio/marker/icons");
  GtkCssProvider *css = gtk_css_provider_new ();
  gtk_css_provider_load_from_resource (css, "/com/github/fabiocolacio/marker/styles/marker.css");
  gtk_style_context_add_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (css),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_test_add_func ("/notebook/editor-apply-cancel", test_editor_apply_cancel);
  g_test_add_func ("/notebook/rail-selection-lifetime", test_rail_selection_and_lifetime);
  int result = g_test_run ();
  marker_session_clear ();
  g_autofree char *directory = g_build_filename (temporary, "marker", NULL);
  g_rmdir (directory);
  g_rmdir (temporary);
  gtk_style_context_remove_provider_for_display (gdk_display_get_default (), GTK_STYLE_PROVIDER (css));
  g_object_unref (css);
  return result;
}
