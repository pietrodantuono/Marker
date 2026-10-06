/* marker-window.c
 *
 * Copyright (C) 2017-2026 Marker contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-window.h"

#include <glib/gi18n.h>

#include "marker-exporter.h"
#include "marker.h"
#include "marker-prefs.h"
#include "marker-project.h"
#include "marker-notebook-editor.h"
#include "marker-notebook-rail.h"
#include "marker-page-sidebar.h"
#include "marker-outline.h"
#include "marker-workspace.h"

struct _MarkerWindow
{
  AdwApplicationWindow parent_instance;

  AdwToolbarView *toolbar_view;
  AdwHeaderBar *header_bar;
  AdwWindowTitle *title;
  AdwOverlaySplitView *split_view;
  AdwOverlaySplitView *rail_split;
  AdwOverlaySplitView *outline_split;
  MarkerOutline *outline_content;
  GtkWidget *notebooks_button;
  GtkWidget *view_button;
  GtkWidget *project_rail;
  MarkerPageSidebar *project_sidebar;
  GtkWidget *save_button;
  GtkWidget *editor_stack;
  GtkWidget *empty_state;
  GtkWidget *loose_files_button;

  GListStore *projects;
  GListStore *documents;
  GtkSingleSelection *project_selection;
  GtkSingleSelection *document_selection;
  MarkerProject *active_project;

  gboolean focus_mode;
  gboolean pages_visible;
  gboolean outline_visible;
  gboolean closing;
  gboolean fullscreen;
};

G_DEFINE_TYPE (MarkerWindow, marker_window, ADW_TYPE_APPLICATION_WINDOW)

static void rebuild_project_tree (MarkerWindow *self);
static void update_loose_files (MarkerWindow *self);
static void update_title (MarkerWindow *self);
static void select_document (MarkerWindow *self,
                             MarkerEditor *editor);
static void remove_editor (MarkerWindow *self,
                           MarkerEditor *editor);
static void save_all_and_close (MarkerWindow *self);
static void remove_project_after_saving (MarkerWindow *self, MarkerProject *project);

static MarkerEditor *
document_at (MarkerWindow *self,
             guint         position)
{
  if (position >= g_list_model_get_n_items (G_LIST_MODEL (self->documents)))
    return NULL;
  return g_list_model_get_item (G_LIST_MODEL (self->documents), position);
}

static MarkerProject *
project_at (MarkerWindow *self,
            guint         position)
{
  if (position >= g_list_model_get_n_items (G_LIST_MODEL (self->projects)))
    return NULL;
  return g_list_model_get_item (G_LIST_MODEL (self->projects), position);
}

static gint
find_document (MarkerWindow *self,
               GFile        *file)
{
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));

  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      GFile *candidate = marker_editor_get_file (editor);
      if (candidate != NULL && g_file_equal (candidate, file))
        return i;
    }
  return -1;
}

static gint
find_project (MarkerWindow *self,
              const char   *uri)
{
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->projects));

  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerProject) project = project_at (self, i);
      if (g_str_equal (marker_project_get_uri (project), uri))
        return i;
    }
  return -1;
}

static void
show_error (MarkerWindow *self,
            const char   *heading,
            const char   *body)
{
  AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (heading, body));
  adw_alert_dialog_add_response (dialog, "close", _("Close"));
  adw_alert_dialog_set_default_response (dialog, "close");
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

static GtkWidget *
make_action_button (const char *icon,
                    const char *tooltip,
                    const char *action)
{
  GtkWidget *button = gtk_button_new_from_icon_name (icon);
  gtk_widget_add_css_class (button, "flat");
  gtk_widget_set_tooltip_text (button, tooltip);
  gtk_actionable_set_action_name (GTK_ACTIONABLE (button), action);
  return button;
}

static void
launch_file (MarkerWindow *self,
             GFile        *file)
{
  g_autofree char *uri = g_file_get_uri (file);
  GtkUriLauncher *launcher = gtk_uri_launcher_new (uri);

  gtk_uri_launcher_launch (launcher, GTK_WINDOW (self), NULL, NULL, NULL);
  g_object_unref (launcher);
}

static void
rebuild_project_tree (MarkerWindow *self)
{
  marker_page_sidebar_set_project (self->project_sidebar, self->active_project);
  marker_page_sidebar_refresh (self->project_sidebar);
}

static void
show_loose_files_cb (GtkButton *button, MarkerWindow *self)
{
  gtk_single_selection_set_selected (self->project_selection, GTK_INVALID_LIST_POSITION);
}

static void
assign_editor_project (MarkerWindow *self, MarkerEditor *editor)
{
  GFile *file = marker_editor_get_file (editor);
  if (file == NULL)
    return;
  MarkerProject *best = NULL;
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->projects)); i++)
    {
      g_autoptr (MarkerProject) project = project_at (self, i);
      if (marker_project_contains (project, file) &&
          (best == NULL || marker_project_contains (best, marker_project_get_root (project))))
        best = project;
    }
  marker_editor_set_project (editor, best);
}

static void
update_title (MarkerWindow *self)
{
  g_autoptr (MarkerEditor) editor = NULL;
  g_autofree char *title = NULL;
  g_autofree char *subtitle = NULL;
  guint position = gtk_single_selection_get_selected (self->document_selection);

  editor = document_at (self, position);
  GSimpleAction *view = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self), "view-mode"));
  g_autofree char *filename = editor != NULL && marker_editor_get_file (editor) != NULL
    ? g_file_get_basename (marker_editor_get_file (editor)) : NULL;
  g_simple_action_set_enabled (view, editor != NULL && (filename == NULL || !g_str_has_suffix (filename, ".css")));
  if (editor != NULL)
    g_simple_action_set_state (view, g_variant_new_string ((const char *[]) {
      "editor", "preview", "dual-pane", "dual-window", "formatted"
    }[marker_editor_get_view_mode (editor)]));
  gtk_widget_set_visible (self->save_button, editor != NULL && marker_editor_has_unsaved_changes (editor));
  if (editor == NULL)
    {
      adw_window_title_set_title (self->title, "Marker");
      adw_window_title_set_subtitle (self->title, NULL);
      return;
    }

  title = marker_editor_get_title (editor);
  subtitle = marker_editor_get_subtitle (editor);
  adw_window_title_set_title (self->title, title);
  adw_window_title_set_subtitle (self->title, NULL);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->title), subtitle);
}

static void
active_editor_changed_cb (GtkSingleSelection *selection,
                          GParamSpec          *pspec,
                          MarkerWindow        *self)
{
  g_autoptr (MarkerEditor) editor = document_at (self,
                                                  gtk_single_selection_get_selected (selection));
  if (editor != NULL)
    {
      const char *mode = (const char *[]) {
        "editor", "preview", "dual-pane", "dual-window", "formatted"
      }[marker_editor_get_view_mode (editor)];
      GSimpleAction *action = G_SIMPLE_ACTION (g_action_map_lookup_action (
        G_ACTION_MAP (self), "view-mode"));
      gtk_stack_set_visible_child (GTK_STACK (self->editor_stack), GTK_WIDGET (editor));
      g_simple_action_set_state (action, g_variant_new_string (mode));
      MarkerProject *project = marker_editor_get_project (editor);
      guint selected = project != NULL ? find_project (self, marker_project_get_uri (project)) : GTK_INVALID_LIST_POSITION;
      gtk_single_selection_set_selected (self->project_selection, selected);
    }
  else
    gtk_stack_set_visible_child (GTK_STACK (self->editor_stack), self->empty_state);
  marker_page_sidebar_set_active (self->project_sidebar, editor);
  marker_outline_set_editor (self->outline_content, editor);
  update_title (self);
}

static void
active_project_changed_cb (GtkSingleSelection *selection, GParamSpec *pspec, MarkerWindow *self)
{
  MarkerProject *project = gtk_single_selection_get_selected_item (selection);
  g_set_object (&self->active_project, project);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (
    G_ACTION_MAP (self), "notebook-styles")), project != NULL && !marker_project_get_missing (project));
  marker_page_sidebar_set_project (self->project_sidebar, project);
  MarkerEditor *active = marker_window_get_active_editor (self);
  if (active == NULL || marker_editor_get_project (active) != project)
    {
      guint selected = GTK_INVALID_LIST_POSITION;
      for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->documents)); i++)
        {
          g_autoptr (MarkerEditor) editor = document_at (self, i);
          if (marker_editor_get_project (editor) == project)
            { selected = i; break; }
        }
      gtk_single_selection_set_selected (self->document_selection, selected);
    }
  update_title (self);
}

static void
page_select_cb (MarkerPageSidebar *sidebar, MarkerEditor *editor, MarkerWindow *self)
{
  select_document (self, editor);
  if (adw_overlay_split_view_get_collapsed (self->split_view))
    adw_overlay_split_view_set_show_sidebar (self->split_view, FALSE);
}

static void
page_close_cb (MarkerPageSidebar *sidebar, MarkerEditor *editor, MarkerWindow *self)
{
  select_document (self, editor);
  marker_window_close_current_document (self);
}

static void
page_open_cb (MarkerPageSidebar *sidebar, GFile *file, gboolean external, MarkerWindow *self)
{
  g_autofree char *name = g_file_get_basename (file);
  g_autofree char *folded = g_ascii_strdown (name, -1);
  if (!external && (g_str_has_suffix (folded, ".md") || g_str_has_suffix (folded, ".markdown") || g_str_has_suffix (folded, ".css")))
    marker_window_new_editor_from_file (self, file);
  else
    launch_file (self, file);
  if (adw_overlay_split_view_get_collapsed (self->split_view))
    adw_overlay_split_view_set_show_sidebar (self->split_view, FALSE);
}

static void
select_document (MarkerWindow *self,
                 MarkerEditor *editor)
{
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));
  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) candidate = document_at (self, i);
      if (candidate == editor)
        {
          gtk_single_selection_set_selected (self->document_selection, i);
          return;
        }
    }
}

static void
editor_title_changed_cb (MarkerEditor *editor,
                         MarkerWindow *self)
{
  assign_editor_project (self, editor);
  if (marker_window_get_active_editor (self) == editor)
    update_title (self);
  update_loose_files (self);
}

static void
add_editor (MarkerWindow *self,
            MarkerEditor *editor)
{
  if (marker_editor_get_file (editor) == NULL)
    marker_editor_set_project (editor, self->active_project);
  else
    assign_editor_project (self, editor);
  g_autofree char *page_name = g_strdup_printf ("editor-%p", editor);
  gtk_stack_add_named (GTK_STACK (self->editor_stack), GTK_WIDGET (editor), page_name);
  g_list_store_append (self->documents, editor);
  g_signal_connect_object (editor, "title-changed", G_CALLBACK (editor_title_changed_cb), self, 0);
  g_signal_connect_object (editor, "subtitle-changed", G_CALLBACK (editor_title_changed_cb), self, 0);
  marker_editor_set_focus_mode (editor, self->focus_mode);
  gtk_single_selection_set_selected (self->document_selection,
                                     g_list_model_get_n_items (G_LIST_MODEL (self->documents)) - 1);
  gtk_stack_set_visible_child (GTK_STACK (self->editor_stack), GTK_WIDGET (editor));
  update_loose_files (self);
}

static void
update_loose_files (MarkerWindow *self)
{
  gboolean has_loose = FALSE;
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));

  for (guint i = 0; i < count && !has_loose; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      has_loose = marker_editor_get_project (editor) == NULL;
    }
  gtk_widget_set_visible (self->loose_files_button, has_loose);
}

static void
open_files_finished_cb (GObject      *source,
                        GAsyncResult *result,
                        gpointer      user_data)
{
  g_autoptr (MarkerWindow) self = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GListModel) files = gtk_file_dialog_open_multiple_finish (
    GTK_FILE_DIALOG (source), result, &error);
  if (self->documents == NULL)
    return;

  if (files == NULL)
    {
      if (error != NULL && !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
          !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
        show_error (self, _("Unable to open file"), error->message);
      return;
    }
  for (guint i = 0; i < g_list_model_get_n_items (files); i++)
    {
      g_autoptr (GFile) file = g_list_model_get_item (files, i);
      marker_window_new_editor_from_file (self, file);
    }
}

static void
action_open_file (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  GtkFileFilter *filter = gtk_file_filter_new ();
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);

  gtk_file_dialog_set_title (dialog, _("Open Markdown Files"));
  gtk_file_filter_set_name (filter, _("Markdown"));
  gtk_file_filter_add_pattern (filter, "*.md");
  gtk_file_filter_add_pattern (filter, "*.markdown");
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_set_default_filter (dialog, filter);
  gtk_file_dialog_open_multiple (dialog, GTK_WINDOW (self), NULL,
                                 open_files_finished_cb, g_object_ref (self));
  g_object_unref (filter);
  g_object_unref (filters);
  g_object_unref (dialog);
}

static void
open_folder_finished_cb (GObject      *source,
                         GAsyncResult *result,
                         gpointer      user_data)
{
  g_autoptr (MarkerWindow) self = user_data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) folder = gtk_file_dialog_select_folder_finish (
    GTK_FILE_DIALOG (source), result, &error);
  if (self->projects == NULL)
    return;

  if (folder != NULL)
    marker_window_add_project (self, folder);
  else if (error != NULL && !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
           !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
    show_error (self, _("Unable to open folder"), error->message);
}

static void
action_open_folder (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Open Notebook"));
  gtk_file_dialog_select_folder (dialog, GTK_WINDOW (self), NULL,
                                 open_folder_finished_cb, g_object_ref (self));
  g_object_unref (dialog);
}

static void
action_new_document (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
  marker_window_new_editor (MARKER_WINDOW (user_data));
}

typedef struct
{
  MarkerWindow *window;
  char *name;
} CreateNotebookRequest;

static void
create_notebook_finished_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  CreateNotebookRequest *request = data;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) parent = gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (source), result, &error);
  if (parent != NULL && request->window->projects != NULL)
    {
      g_autoptr (GFile) root = g_file_get_child (parent, request->name);
      if (g_file_make_directory (root, NULL, &error))
        marker_window_add_project (request->window, root);
      else
        show_error (request->window, _("Unable to create notebook"), error->message);
    }
  g_object_unref (request->window);
  g_free (request->name);
  g_free (request);
}

static void
new_notebook_response_cb (AdwAlertDialog *dialog, const char *response, MarkerWindow *self)
{
  if (!g_str_equal (response, "create"))
    return;
  GtkEditable *entry = g_object_get_data (G_OBJECT (dialog), "name");
  g_autofree char *name = g_strdup (gtk_editable_get_text (entry));
  g_strstrip (name);
  if (*name == '\0' || strchr (name, G_DIR_SEPARATOR) != NULL || g_str_equal (name, ".") || g_str_equal (name, ".."))
    {
      show_error (self, _("Choose a notebook name"), _("Use a folder name without a path separator."));
      return;
    }
  CreateNotebookRequest *request = g_new0 (CreateNotebookRequest, 1);
  request->window = g_object_ref (self);
  request->name = g_steal_pointer (&name);
  GtkFileDialog *chooser = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (chooser, _("Choose Notebook Location"));
  gtk_file_dialog_select_folder (chooser, GTK_WINDOW (self), NULL, create_notebook_finished_cb, request);
  g_object_unref (chooser);
}

static void
action_new_notebook (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (_("New Notebook"), _("Create a folder for your pages and assets.")));
  GtkWidget *name = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (name), _("Notebook Name"));
  gtk_editable_set_text (GTK_EDITABLE (name), _("Notebook"));
  g_object_set_data (G_OBJECT (dialog), "name", name);
  adw_alert_dialog_set_extra_child (dialog, name);
  adw_alert_dialog_add_responses (dialog, "cancel", _("Cancel"), "create", _("Choose Location…"), NULL);
  adw_alert_dialog_set_close_response (dialog, "cancel");
  adw_alert_dialog_set_default_response (dialog, "create");
  adw_alert_dialog_set_response_appearance (dialog, "create", ADW_RESPONSE_SUGGESTED);
  g_signal_connect_object (dialog, "response", G_CALLBACK (new_notebook_response_cb), self, 0);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

typedef struct
{
  MarkerWindow *window;
  MarkerEditor *editor;
  gboolean close_document;
  gboolean close_window;
  MarkerProject *remove_project;
} SaveAsRequest;

static void
save_as_request_free (SaveAsRequest *request)
{
  g_clear_object (&request->window);
  g_clear_object (&request->editor);
  g_clear_object (&request->remove_project);
  g_free (request);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC (SaveAsRequest, save_as_request_free)

static void
save_as_finished_cb (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
  g_autoptr (SaveAsRequest) request = user_data;
  MarkerWindow *self = request->window;
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) file = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source),
                                                        result, &error);
  if (self->documents == NULL)
    return;
  if (file != NULL)
    {
      if (!marker_editor_save_file_as (request->editor, file))
        {
          show_error (self, _("Unable to save file"),
                      _("The document could not be written to the selected location."));
          return;
        }
      update_title (self);
      update_loose_files (self);
      marker_page_sidebar_refresh (self->project_sidebar);
      if (request->close_document)
        remove_editor (self, request->editor);
      else if (request->close_window)
        save_all_and_close (self);
      else if (request->remove_project != NULL)
        remove_project_after_saving (self, request->remove_project);
    }
  else if (error != NULL && !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED) &&
           !g_error_matches (error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_CANCELLED))
    show_error (self, _("Unable to save file"), error->message);
}

static void
save_editor_as (MarkerWindow *self,
                MarkerEditor *editor,
                gboolean      close_document,
                gboolean      close_window,
                MarkerProject *remove_project)
{
  GtkFileDialog *dialog;
  g_autofree char *title = NULL;
  SaveAsRequest *request;

  if (editor == NULL)
    return;
  dialog = gtk_file_dialog_new ();
  title = marker_editor_get_raw_title (editor);
  if (g_str_equal (title, "Untitled"))
    gtk_file_dialog_set_initial_name (dialog, "Untitled.md");
  else
    gtk_file_dialog_set_initial_name (dialog, title);
  gtk_file_dialog_set_title (dialog, _("Save File"));
  MarkerProject *project = marker_editor_get_project (editor);
  if (project != NULL && !marker_project_get_missing (project))
    gtk_file_dialog_set_initial_folder (dialog, marker_project_get_root (project));
  request = g_new0 (SaveAsRequest, 1);
  request->window = g_object_ref (self);
  request->editor = g_object_ref (editor);
  request->close_document = close_document;
  request->close_window = close_window;
  request->remove_project = remove_project != NULL ? g_object_ref (remove_project) : NULL;
  gtk_file_dialog_save (dialog, GTK_WINDOW (self), NULL, save_as_finished_cb,
                        request);
  g_object_unref (dialog);
}

void
marker_window_save_active_file_as (MarkerWindow *self)
{
  save_editor_as (self, marker_window_get_active_editor (self), FALSE, FALSE, NULL);
}

void
marker_window_save_active_file (MarkerWindow *self)
{
  MarkerEditor *editor = marker_window_get_active_editor (self);
  if (editor == NULL)
    return;
  if (marker_editor_get_file (editor) == NULL)
    marker_window_save_active_file_as (self);
  else
    marker_editor_save_file (editor);
  update_title (self);
}

static MarkerViewMode
view_mode_from_name (const char *name)
{
  if (g_str_equal (name, "preview"))
    return PREVIEW_ONLY_MODE;
  if (g_str_equal (name, "dual-pane"))
    return DUAL_PANE_MODE;
  if (g_str_equal (name, "dual-window"))
    return DUAL_WINDOW_MODE;
  if (g_str_equal (name, "formatted"))
    return FORMATTED_MODE;
  return EDITOR_ONLY_MODE;
}

static void
action_view_mode (GSimpleAction *action,
                  GVariant      *parameter,
                  gpointer       user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  const char *name = g_variant_get_string (parameter, NULL);
  MarkerEditor *editor = marker_window_get_active_editor (self);

  if (editor != NULL)
    {
      marker_editor_set_view_mode (editor, view_mode_from_name (name));
      const char *actual = (const char *[]) { "editor", "preview", "dual-pane", "dual-window", "formatted" }
        [marker_editor_get_view_mode (editor)];
      g_simple_action_set_state (action, g_variant_new_string (actual));
    }
}

static void
sync_navigation (MarkerWindow *self)
{
  gboolean narrow = adw_overlay_split_view_get_collapsed (self->rail_split);
  gtk_widget_set_visible (self->notebooks_button, narrow);
  gtk_widget_set_visible (self->view_button, !narrow);
  adw_overlay_split_view_set_show_sidebar (self->rail_split, !narrow && !self->focus_mode);
  adw_overlay_split_view_set_show_sidebar (self->split_view,
    self->pages_visible && !self->focus_mode && !adw_overlay_split_view_get_collapsed (self->split_view));
  adw_overlay_split_view_set_show_sidebar (self->outline_split,
    self->outline_visible && !self->focus_mode && !adw_overlay_split_view_get_collapsed (self->outline_split));
}

static void
collapsed_changed_cb (AdwOverlaySplitView *split, GParamSpec *pspec, MarkerWindow *self)
{
  sync_navigation (self);
}

static void
sidebar_shown_cb (AdwOverlaySplitView *split, GParamSpec *pspec, MarkerWindow *self)
{
  AdwOverlaySplitView *surfaces[] = { self->rail_split, self->split_view, self->outline_split };
  if (adw_overlay_split_view_get_collapsed (split) && adw_overlay_split_view_get_show_sidebar (split))
    for (guint i = 0; i < G_N_ELEMENTS (surfaces); i++)
      if (surfaces[i] != split && adw_overlay_split_view_get_collapsed (surfaces[i]))
        adw_overlay_split_view_set_show_sidebar (surfaces[i], FALSE);
  if (split == self->outline_split)
    {
      GSimpleAction *action = G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (self), "outline"));
      if (action != NULL)
        g_simple_action_set_state (action, g_variant_new_boolean (adw_overlay_split_view_get_show_sidebar (split)));
    }
}

static void
action_sidebar (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  self->pages_visible = !adw_overlay_split_view_get_show_sidebar (self->split_view);
  adw_overlay_split_view_set_show_sidebar (self->split_view, self->pages_visible);
}

static void
action_notebooks (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  adw_overlay_split_view_set_show_sidebar (self->rail_split,
    !adw_overlay_split_view_get_show_sidebar (self->rail_split));
}

static void
action_outline (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  self->outline_visible = !adw_overlay_split_view_get_show_sidebar (self->outline_split);
  adw_overlay_split_view_set_show_sidebar (self->outline_split, self->outline_visible);
}

static void
action_outline_close (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  self->outline_visible = FALSE;
  adw_overlay_split_view_set_show_sidebar (self->outline_split, FALSE);
}

static void
set_focus_mode (MarkerWindow *self, gboolean enabled)
{
  self->focus_mode = enabled;
  sync_navigation (self);
  adw_toolbar_view_set_reveal_top_bars (self->toolbar_view, !enabled);
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->documents)); i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      marker_editor_set_focus_mode (editor, enabled);
    }
}

static void
action_focus (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  gboolean enabled = !self->focus_mode;
  g_simple_action_set_state (action, g_variant_new_boolean (enabled));
  set_focus_mode (self, enabled);
  g_settings_set_boolean (prefs.window_settings, "focus-mode", enabled);
}

static void
action_escape (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  if (self->focus_mode)
    {
      GAction *focus = g_action_map_lookup_action (G_ACTION_MAP (self), "focus");
      g_action_activate (focus, NULL);
    }
}

static void
action_search (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  MarkerEditor *editor = marker_window_get_active_editor (MARKER_WINDOW (user_data));
  if (editor != NULL)
    marker_editor_toggle_search_bar (editor);
}

static void
action_save (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  marker_window_save_active_file (MARKER_WINDOW (user_data));
}

static void
action_save_as (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
  marker_window_save_active_file_as (MARKER_WINDOW (user_data));
}

static void
action_reload (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  MarkerEditor *editor = marker_window_get_active_editor (MARKER_WINDOW (user_data));
  if (editor != NULL)
    marker_editor_reload_file (editor);
}

static void
action_close_document (GSimpleAction *action,
                       GVariant      *parameter,
                       gpointer       user_data)
{
  marker_window_close_current_document (MARKER_WINDOW (user_data));
}

static void
action_fullscreen (GSimpleAction *action,
                   GVariant      *parameter,
                   gpointer       user_data)
{
  marker_window_toggle_fullscreen (MARKER_WINDOW (user_data));
}

static void
action_preferences (GSimpleAction *action,
                    GVariant      *parameter,
                    gpointer       user_data)
{
  marker_prefs_show_window ();
}

static void
action_export (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  marker_exporter_show_export_dialog (MARKER_WINDOW (user_data));
}

static void
action_print (GSimpleAction *action,
              GVariant      *parameter,
              gpointer       user_data)
{
  MarkerEditor *editor = marker_window_get_active_editor (MARKER_WINDOW (user_data));
  if (editor != NULL)
    marker_editor_print (editor, GTK_WINDOW (user_data));
}

static void
action_format (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
  MarkerEditor *editor = marker_window_get_active_editor (MARKER_WINDOW (user_data));
  const char *name = g_action_get_name (G_ACTION (action));
  if (editor == NULL)
    return;
  if (g_str_equal (name, "bold"))
    marker_editor_format_selection (editor, "**");
  else if (g_str_equal (name, "italic"))
    marker_editor_format_selection (editor, "*");
  else if (g_str_equal (name, "monospace"))
    marker_editor_format_selection (editor, "`");
  else if (g_str_equal (name, "link"))
    marker_editor_format_selection (editor, "link");
}

static void
action_sidebar_refresh (GSimpleAction *action,
                        GVariant      *parameter,
                        gpointer       user_data)
{
  rebuild_project_tree (MARKER_WINDOW (user_data));
}

static void
project_changed_cb (MarkerProject *project, GParamSpec *pspec, MarkerWindow *self)
{
  if (self->active_project == project)
    {
      if (g_str_equal (pspec->name, "root"))
        rebuild_project_tree (self);
      update_title (self);
    }
}

static void
project_styles_changed_cb (MarkerProject *project, MarkerWindow *self)
{
  if (self->active_project == project)
    marker_page_sidebar_refresh (self->project_sidebar);
}


static void
move_active_project (MarkerWindow *self,
                     gint          direction)
{
  guint position = gtk_single_selection_get_selected (self->project_selection);
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->projects));
  gint target = (gint) position + direction;
  g_autoptr (MarkerProject) current = NULL;
  g_autoptr (MarkerProject) other = NULL;
  gpointer reordered[2];
  guint start;

  if (position == GTK_INVALID_LIST_POSITION || target < 0 || target >= (gint) count)
    return;
  current = project_at (self, position);
  other = project_at (self, target);
  start = MIN (position, (guint) target);
  if (direction < 0)
    {
      reordered[0] = current;
      reordered[1] = other;
    }
  else
    {
      reordered[0] = other;
      reordered[1] = current;
    }
  g_list_store_splice (self->projects, start, 2, reordered, 2);
  gtk_single_selection_set_selected (self->project_selection, target);
}




typedef struct
{
  MarkerWindow *window;
  MarkerProject *project;
} NotebookRequest;

static void
notebook_request_free (NotebookRequest *request)
{
  g_object_unref (request->window);
  g_object_unref (request->project);
  g_free (request);
}

static NotebookRequest *
notebook_request_new (MarkerWindow *self, MarkerProject *project)
{
  NotebookRequest *request = g_new0 (NotebookRequest, 1);
  request->window = g_object_ref (self);
  request->project = g_object_ref (project);
  return request;
}

static void
notebook_request_closure_free (gpointer data, GClosure *closure)
{
  notebook_request_free (data);
}

static void
remove_project_now (MarkerWindow *self, MarkerProject *project)
{
  gint selected = find_project (self, marker_project_get_uri (project));
  if (selected < 0)
    return;
  for (gint i = g_list_model_get_n_items (G_LIST_MODEL (self->documents)) - 1; i >= 0; i--)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      if (marker_editor_get_project (editor) == project)
        remove_editor (self, editor);
    }
  g_list_store_remove (self->projects, selected);
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->projects));
  if (count > 0)
    gtk_single_selection_set_selected (self->project_selection, MIN ((guint) selected, count - 1));
  else
    {
      g_clear_object (&self->active_project);
      rebuild_project_tree (self);
    }
  update_loose_files (self);
}

static void
remove_project_after_saving (MarkerWindow *self, MarkerProject *project)
{
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->documents)); i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      if (marker_editor_get_project (editor) != project || !marker_editor_has_unsaved_changes (editor))
        continue;
      if (marker_editor_get_file (editor) == NULL)
        {
          save_editor_as (self, editor, FALSE, FALSE, project);
          return;
        }
      if (!marker_editor_save_file (editor))
        {
          show_error (self, _("Unable to save page"), _("The notebook and its open pages remain in Marker."));
          return;
        }
    }
  remove_project_now (self, project);
}

static void
remove_project_response_cb (AdwAlertDialog *dialog, const char *response, NotebookRequest *request)
{
  if (g_str_equal (response, "save"))
    remove_project_after_saving (request->window, request->project);
  else if (g_str_equal (response, "remove"))
    remove_project_now (request->window, request->project);
}

static void
remove_project (MarkerWindow *self, MarkerProject *project)
{
  gboolean dirty = FALSE;
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));
  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      if (marker_editor_get_project (editor) == project && marker_editor_has_unsaved_changes (editor))
        dirty = TRUE;
    }
  AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (
    _("Remove Notebook?"), dirty
      ? _("This notebook contains unsaved pages. Save or discard their changes before removing it. Its folder will not be deleted.")
      : _("Its open pages will close. Its folder and files will not be deleted.")));
  adw_alert_dialog_add_responses (dialog, "cancel", _("Cancel"),
                                  "remove", dirty ? _("Discard and Remove") : _("Remove"), NULL);
  if (dirty)
    {
      adw_alert_dialog_add_response (dialog, "save", _("Save and Remove"));
      adw_alert_dialog_set_response_appearance (dialog, "save", ADW_RESPONSE_SUGGESTED);
    }
  adw_alert_dialog_set_response_appearance (dialog, "remove", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_default_response (dialog, "cancel");
  adw_alert_dialog_set_close_response (dialog, "cancel");
  g_signal_connect_data (dialog, "response", G_CALLBACK (remove_project_response_cb),
                         notebook_request_new (self, project), notebook_request_closure_free, 0);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}


static void
locate_project_finished_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
  NotebookRequest *request = user_data;
  g_autoptr (GFile) folder = gtk_file_dialog_select_folder_finish (GTK_FILE_DIALOG (source), result, NULL);
  if (folder != NULL && request->window->projects != NULL)
    {
      gint existing = find_project (request->window, marker_project_get_uri (request->project));
      g_autofree char *uri = g_file_get_uri (folder);
      if (existing >= 0 && find_project (request->window, uri) < 0)
        {
          marker_project_relocate (request->project, folder);
          marker_session_store_project (request->project, NULL);
        }
    }
  notebook_request_free (request);
}

static void
locate_project (MarkerWindow *self, MarkerProject *project)
{
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  gtk_file_dialog_set_title (dialog, _("Locate Notebook Folder"));
  gtk_file_dialog_select_folder (dialog, GTK_WINDOW (self), NULL,
                                 locate_project_finished_cb, notebook_request_new (self, project));
  g_object_unref (dialog);
}


static void
edit_notebook_styles (MarkerWindow *self, MarkerProject *project)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) style = marker_project_ensure_stylesheet (project, &error);
  if (style == NULL)
    {
      show_error (self, _("Unable to open notebook styles"), error->message);
      return;
    }
  gtk_single_selection_set_selected (self->project_selection,
                                     find_project (self, marker_project_get_uri (project)));
  marker_page_sidebar_set_files (self->project_sidebar, TRUE);
  marker_window_new_editor_from_file (self, style);
  marker_page_sidebar_refresh (self->project_sidebar);
}

static void
action_notebook_styles (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
  MarkerWindow *self = MARKER_WINDOW (user_data);
  if (self->active_project != NULL)
    edit_notebook_styles (self, self->active_project);
}

static void
notebook_action_cb (MarkerNotebookRail *rail, const char *action, MarkerProject *project, MarkerWindow *self)
{
  gint index = find_project (self, marker_project_get_uri (project));
  if (index < 0)
    return;
  g_autoptr (MarkerProject) target = g_object_ref (project);
  if (g_str_equal (action, "edit"))
    marker_notebook_editor_present (target, GTK_WIDGET (self));
  else if (g_str_equal (action, "reveal"))
    launch_file (self, marker_project_get_root (target));
  else if (g_str_equal (action, "remove"))
    remove_project (self, target);
  else if (g_str_equal (action, "locate"))
    locate_project (self, target);
  else if (g_str_equal (action, "styles"))
    edit_notebook_styles (self, target);
  else if (g_str_equal (action, "move-up") || g_str_equal (action, "move-down"))
    {
      gtk_single_selection_set_selected (self->project_selection, index);
      move_active_project (self, g_str_equal (action, "move-up") ? -1 : 1);
    }
}

static void
action_sort (GSimpleAction *action,
             GVariant      *parameter,
             gpointer       user_data)
{
  const char *sort = g_variant_get_string (parameter, NULL);
  g_simple_action_set_state (action, parameter);
  g_settings_set_string (prefs.window_settings, "sidebar-sort", sort);
  rebuild_project_tree (MARKER_WINDOW (user_data));
}

static const GActionEntry window_actions[] = {
  { "new-document", action_new_document },
  { "new-notebook", action_new_notebook },
  { "open-file", action_open_file },
  { "open-folder", action_open_folder },
  { "save", action_save },
  { "save-as", action_save_as },
  { "reload", action_reload },
  { "close-document", action_close_document },
  { "search", action_search },
  { "sidebar", action_sidebar },
  { "notebooks", action_notebooks },
  { "outline", action_outline, NULL, "false" },
  { "outline-close", action_outline_close },
  { "fullscreen", action_fullscreen },
  { "preferences", action_preferences },
  { "export", action_export },
  { "print", action_print },
  { "bold", action_format },
  { "italic", action_format },
  { "monospace", action_format },
  { "link", action_format },
  { "sidebar-refresh", action_sidebar_refresh },
  { "notebook-styles", action_notebook_styles },
  { "escape", action_escape },
  { "view-mode", action_view_mode, "s", "'formatted'" },
  { "focus", action_focus, NULL, "false" },
  { "sort", action_sort, "s", "'name'" },
};

static void
menu_append_target (GMenu      *menu,
                    const char *label,
                    const char *action,
                    const char *target)
{
  GMenuItem *item = g_menu_item_new (label, NULL);
  g_menu_item_set_action_and_target (item, action, "s", target);
  g_menu_append_item (menu, item);
  g_object_unref (item);
}

static GtkWidget *
create_header_bar (MarkerWindow *self)
{
  AdwHeaderBar *bar = ADW_HEADER_BAR (adw_header_bar_new ());
  GtkWidget *pages = make_action_button ("sidebar-show-symbolic", _("Pages"), "win.sidebar");
  GtkWidget *outline = gtk_toggle_button_new ();
  GtkWidget *overflow = gtk_menu_button_new ();
  self->notebooks_button = make_action_button ("open-menu-symbolic", _("Notebooks"), "win.notebooks");
  self->view_button = gtk_menu_button_new ();
  self->save_button = make_action_button ("document-save-symbolic", _("Save Page"), "win.save");
  self->title = ADW_WINDOW_TITLE (adw_window_title_new ("Marker", NULL));
  adw_header_bar_set_title_widget (bar, GTK_WIDGET (self->title));
  gtk_widget_add_css_class (GTK_WIDGET (bar), "marker-topbar");
  gtk_button_set_icon_name (GTK_BUTTON (outline), "view-list-symbolic");
  gtk_widget_add_css_class (outline, "flat");
  gtk_widget_add_css_class (self->view_button, "flat");
  gtk_widget_add_css_class (overflow, "flat");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (outline), "win.outline");
  gtk_widget_set_tooltip_text (outline, _("Outline"));
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (self->view_button), "view-grid-symbolic");
  gtk_widget_set_tooltip_text (self->view_button, _("View Mode"));
  g_autoptr (GMenu) view = g_menu_new ();
  const char *labels[] = { _("Editor Only"), _("Preview Only"), _("Dual Pane"), _("Dual Window"), _("Formatted Markdown") };
  const char *modes[] = { "editor", "preview", "dual-pane", "dual-window", "formatted" };
  for (guint i = 0; i < G_N_ELEMENTS (modes); i++)
    menu_append_target (view, labels[i], "win.view-mode", modes[i]);
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (self->view_button), G_MENU_MODEL (view));
  g_autoptr (GMenu) menu = g_menu_new ();
  g_autoptr (GMenu) document = g_menu_new ();
  g_autoptr (GMenu) navigation = g_menu_new ();
  g_menu_append (document, _("Save"), "win.save");
  g_menu_append (document, _("Save As…"), "win.save-as");
  g_menu_append (document, _("Reload from Disk"), "win.reload");
  g_menu_append (document, _("Export…"), "win.export");
  g_menu_append (document, _("Print…"), "win.print");
  g_menu_append (document, _("Find in Source…"), "win.search");
  g_menu_append (document, _("Close Page"), "win.close-document");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (document));
  g_menu_append_submenu (navigation, _("View Mode"), G_MENU_MODEL (view));
  g_menu_append (navigation, _("Pages"), "win.sidebar");
  g_menu_append (navigation, _("Outline"), "win.outline");
  g_menu_append (navigation, _("Focus Mode"), "win.focus");
  g_menu_append (navigation, _("Fullscreen"), "win.fullscreen");
  g_menu_append_section (menu, NULL, G_MENU_MODEL (navigation));
  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (overflow), "view-more-symbolic");
  gtk_widget_set_tooltip_text (overflow, _("Page Menu"));
  gtk_menu_button_set_menu_model (GTK_MENU_BUTTON (overflow), G_MENU_MODEL (menu));
  adw_header_bar_pack_start (bar, self->notebooks_button);
  adw_header_bar_pack_start (bar, pages);
  adw_header_bar_pack_end (bar, overflow);
  adw_header_bar_pack_end (bar, outline);
  adw_header_bar_pack_end (bar, self->view_button);
  adw_header_bar_pack_end (bar, self->save_button);
  self->header_bar = bar;
  return GTK_WIDGET (bar);
}

static GtkWidget *
create_project_rail (MarkerWindow *self)
{
  self->project_selection = gtk_single_selection_new (G_LIST_MODEL (g_object_ref (self->projects)));
  gtk_single_selection_set_autoselect (self->project_selection, TRUE);
  gtk_single_selection_set_can_unselect (self->project_selection, TRUE);
  GtkWidget *rail = marker_notebook_rail_new (self->project_selection);
  g_signal_connect_object (self->project_selection, "notify::selected",
                            G_CALLBACK (active_project_changed_cb), self, 0);
  g_signal_connect_object (rail, "notebook-action", G_CALLBACK (notebook_action_cb), self, 0);
  self->loose_files_button = gtk_button_new_from_icon_name ("document-open-symbolic");
  gtk_widget_set_tooltip_text (self->loose_files_button, _("Open Files"));
  gtk_widget_add_css_class (self->loose_files_button, "flat");
  gtk_widget_set_visible (self->loose_files_button, FALSE);
  g_signal_connect_object (self->loose_files_button, "clicked", G_CALLBACK (show_loose_files_cb), self, 0);
  gtk_box_append (GTK_BOX (rail), self->loose_files_button);
  self->project_rail = rail;
  return rail;
}

static GtkWidget *
create_sidebar (MarkerWindow *self)
{
  self->document_selection = gtk_single_selection_new (G_LIST_MODEL (g_object_ref (self->documents)));
  gtk_single_selection_set_can_unselect (self->document_selection, TRUE);
  gtk_single_selection_set_autoselect (self->document_selection, FALSE);
  g_signal_connect_object (self->document_selection, "notify::selected", G_CALLBACK (active_editor_changed_cb), self, 0);
  self->project_sidebar = MARKER_PAGE_SIDEBAR (marker_page_sidebar_new (G_LIST_MODEL (self->documents)));
  g_signal_connect_object (self->project_sidebar, "select-page", G_CALLBACK (page_select_cb), self, 0);
  g_signal_connect_object (self->project_sidebar, "close-page", G_CALLBACK (page_close_cb), self, 0);
  g_signal_connect_object (self->project_sidebar, "open-file", G_CALLBACK (page_open_cb), self, 0);
  return GTK_WIDGET (self->project_sidebar);
}

static GtkWidget *
create_editor_stack (MarkerWindow *self)
{
  GtkWidget *stack = gtk_stack_new ();
  GtkWidget *status = adw_status_page_new ();
  GtkWidget *new_button = gtk_button_new_with_label (_("New Document"));

  adw_status_page_set_icon_name (ADW_STATUS_PAGE (status), "accessories-text-editor-symbolic");
  adw_status_page_set_title (ADW_STATUS_PAGE (status), _("Ready to write"));
  adw_status_page_set_description (ADW_STATUS_PAGE (status),
                                   _("Create a document or open a Markdown file or project."));
  gtk_widget_add_css_class (new_button, "suggested-action");
  gtk_actionable_set_action_name (GTK_ACTIONABLE (new_button), "win.new-document");
  adw_status_page_set_child (ADW_STATUS_PAGE (status), new_button);
  gtk_widget_add_css_class (status, "marker-empty-state");
  gtk_stack_add_named (GTK_STACK (stack), status, "empty");
  gtk_stack_set_visible_child (GTK_STACK (stack), status);
  gtk_widget_set_hexpand (stack, TRUE);
  gtk_widget_set_vexpand (stack, TRUE);
  self->empty_state = status;
  self->editor_stack = stack;
  return stack;
}

static void
install_breakpoints (MarkerWindow *self)
{
  GValue value = G_VALUE_INIT;
  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, TRUE);
  const int widths[] = { 1099, 699 };
  for (guint i = 0; i < G_N_ELEMENTS (widths); i++)
    {
      AdwBreakpoint *point = adw_breakpoint_new (adw_breakpoint_condition_new_length (
        ADW_BREAKPOINT_CONDITION_MAX_WIDTH, widths[i], ADW_LENGTH_UNIT_PX));
      adw_breakpoint_add_setter (point, G_OBJECT (self->split_view), "collapsed", &value);
      adw_breakpoint_add_setter (point, G_OBJECT (self->outline_split), "collapsed", &value);
      if (i == 1)
        adw_breakpoint_add_setter (point, G_OBJECT (self->rail_split), "collapsed", &value);
      adw_application_window_add_breakpoint (ADW_APPLICATION_WINDOW (self), point);
    }
  g_value_unset (&value);
  AdwOverlaySplitView *surfaces[] = { self->rail_split, self->split_view, self->outline_split };
  for (guint i = 0; i < G_N_ELEMENTS (surfaces); i++)
    {
      g_signal_connect_object (surfaces[i], "notify::collapsed", G_CALLBACK (collapsed_changed_cb), self, 0);
      g_signal_connect_object (surfaces[i], "notify::show-sidebar", G_CALLBACK (sidebar_shown_cb), self, 0);
    }
}

static AdwOverlaySplitView *
new_split (GtkPackType position, double width)
{
  AdwOverlaySplitView *split = ADW_OVERLAY_SPLIT_VIEW (adw_overlay_split_view_new ());
  gtk_widget_add_css_class (GTK_WIDGET (split), "marker-split");
  adw_overlay_split_view_set_sidebar_position (split, position);
  adw_overlay_split_view_set_sidebar_width_unit (split, ADW_LENGTH_UNIT_PX);
  adw_overlay_split_view_set_min_sidebar_width (split, width);
  adw_overlay_split_view_set_max_sidebar_width (split, width);
  return split;
}

static GtkWidget *
create_outline_surface (MarkerWindow *self)
{
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  GtkWidget *header = adw_header_bar_new ();
  adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (header), FALSE);
  adw_header_bar_set_title_widget (ADW_HEADER_BAR (header), gtk_label_new (_("Outline")));
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header), TRUE);
  adw_header_bar_pack_start (ADW_HEADER_BAR (header), make_action_button (
    "window-close-symbolic", _("Close Outline"), "win.outline-close"));
  gtk_box_append (GTK_BOX (box), header);
  self->outline_content = marker_outline_new ();
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->outline_content));
  gtk_widget_add_css_class (box, "marker-outline");
  return box;
}

static void
close_window_response_cb (AdwAlertDialog *dialog,
                          const char     *response,
                          MarkerWindow   *self)
{
  if (g_str_equal (response, "discard"))
    {
      self->closing = TRUE;
      gtk_window_destroy (GTK_WINDOW (self));
    }
  else if (g_str_equal (response, "save"))
    save_all_and_close (self);
}

static gboolean
close_request_cb (GtkWindow    *window,
                  MarkerWindow *self)
{
  marker_save_session ();
  if (self->closing)
    return FALSE;
  return !marker_window_try_close (self);
}

static void
marker_window_dispose (GObject *object)
{
  MarkerWindow *self = MARKER_WINDOW (object);

  if (self->projects == NULL)
    {
      G_OBJECT_CLASS (marker_window_parent_class)->dispose (object);
      return;
    }
  int width = gtk_widget_get_width (GTK_WIDGET (self));
  int height = gtk_widget_get_height (GTK_WIDGET (self));

  if (width > 0 && height > 0 && !self->fullscreen)
    {
      marker_prefs_set_window_width (width);
      marker_prefs_set_window_height (height);
    }
  guint count = self->documents != NULL
                  ? g_list_model_get_n_items (G_LIST_MODEL (self->documents)) : 0;
  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      g_signal_handlers_disconnect_by_data (editor, self);
      marker_editor_closing (editor);
    }
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->projects)); i++)
    {
      g_autoptr (MarkerProject) project = project_at (self, i);
      g_signal_handlers_disconnect_by_data (project, self);
    }
  g_clear_object (&self->active_project);
  g_clear_object (&self->project_selection);
  g_clear_object (&self->document_selection);
  g_clear_object (&self->projects);
  g_clear_object (&self->documents);
  G_OBJECT_CLASS (marker_window_parent_class)->dispose (object);
}

static void
marker_window_class_init (MarkerWindowClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = marker_window_dispose;
}

static void
marker_window_init (MarkerWindow *self)
{
  MarkerViewMode default_mode;
  g_autofree char *sort = NULL;
  /* Breakpoint layouts do not compute a minimum from their collapsed children. */
  gtk_widget_set_size_request (GTK_WIDGET (self), 360, 360);
  self->projects = g_list_store_new (MARKER_TYPE_PROJECT);
  self->documents = g_list_store_new (MARKER_TYPE_EDITOR);
  self->pages_visible = marker_prefs_get_show_sidebar ();
  self->toolbar_view = ADW_TOOLBAR_VIEW (adw_toolbar_view_new ());
  self->split_view = new_split (GTK_PACK_START, 240);
  self->rail_split = new_split (GTK_PACK_START, 48);
  gtk_widget_add_css_class (GTK_WIDGET (self->rail_split), "marker-rail-split");
  self->outline_split = new_split (GTK_PACK_END, 240);
  adw_toolbar_view_add_top_bar (self->toolbar_view, create_header_bar (self));
  adw_toolbar_view_set_content (self->toolbar_view, create_editor_stack (self));
  adw_overlay_split_view_set_sidebar (self->rail_split, create_project_rail (self));
  adw_overlay_split_view_set_sidebar (self->split_view, create_sidebar (self));
  adw_overlay_split_view_set_sidebar (self->outline_split, create_outline_surface (self));
  adw_overlay_split_view_set_content (self->outline_split, GTK_WIDGET (self->toolbar_view));
  adw_overlay_split_view_set_content (self->split_view, GTK_WIDGET (self->outline_split));
  adw_overlay_split_view_set_content (self->rail_split, GTK_WIDGET (self->split_view));
  adw_application_window_set_content (ADW_APPLICATION_WINDOW (self), GTK_WIDGET (self->rail_split));

  g_action_map_add_action_entries (G_ACTION_MAP (self), window_actions,
                                   G_N_ELEMENTS (window_actions), self);
  g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (
    G_ACTION_MAP (self), "notebook-styles")), FALSE);
  default_mode = marker_prefs_get_default_view_mode ();
  if (default_mode < EDITOR_ONLY_MODE || default_mode > FORMATTED_MODE)
    default_mode = FORMATTED_MODE;
  g_simple_action_set_state (G_SIMPLE_ACTION (g_action_map_lookup_action (
    G_ACTION_MAP (self), "view-mode")),
    g_variant_new_string ((const char *[]) {
      "editor", "preview", "dual-pane", "dual-window", "formatted"
    }[default_mode]));
  sort = g_settings_get_string (prefs.window_settings, "sidebar-sort");
  g_simple_action_set_state (G_SIMPLE_ACTION (g_action_map_lookup_action (
    G_ACTION_MAP (self), "sort")), g_variant_new_string (sort));
  gtk_window_set_default_size (GTK_WINDOW (self),
                               marker_prefs_get_window_width (),
                               marker_prefs_get_window_height ());
  gtk_window_set_title (GTK_WINDOW (self), "Marker");
  g_signal_connect (self, "close-request", G_CALLBACK (close_request_cb), self);
  install_breakpoints (self);
  sync_navigation (self);
  if (g_settings_get_boolean (prefs.window_settings, "focus-mode"))
    {
      g_simple_action_set_state (G_SIMPLE_ACTION (g_action_map_lookup_action (
        G_ACTION_MAP (self), "focus")), g_variant_new_boolean (TRUE));
      set_focus_mode (self, TRUE);
    }
}

MarkerWindow *
marker_window_new (GtkApplication *app)
{
  return g_object_new (MARKER_TYPE_WINDOW, "application", app, NULL);
}

MarkerWindow *
marker_window_new_from_file (GtkApplication *app,
                             GFile          *file)
{
  MarkerWindow *self = marker_window_new (app);
  marker_window_new_editor_from_file (self, file);
  return self;
}

MarkerWindow *
marker_window_new_from_workspace (GtkApplication *app,
                                  GFile          *folder,
                                  GError        **error)
{
  MarkerWindow *self;

  if (g_file_query_file_type (folder, G_FILE_QUERY_INFO_NONE, NULL) != G_FILE_TYPE_DIRECTORY)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_DIRECTORY,
                           "The selected location is not a folder.");
      return NULL;
    }
  self = marker_window_new (app);
  marker_window_add_project (self, folder);
  return self;
}

void
marker_window_add_project (MarkerWindow *self,
                           GFile        *folder)
{
  g_autoptr (MarkerProject) project = marker_project_new (folder);
  gint existing = find_project (self, marker_project_get_uri (project));

  if (existing >= 0)
    {
      gtk_single_selection_set_selected (self->project_selection, existing);
      return;
    }

  /* Reuse the model already open in another window, so appearance cannot drift. */
  GtkApplication *app = gtk_window_get_application (GTK_WINDOW (self));
  gboolean shared_model = FALSE;
  for (GList *l = gtk_application_get_windows (app); l != NULL; l = l->next)
    {
      if (!MARKER_IS_WINDOW (l->data) || l->data == self)
        continue;
      MarkerWindow *other = l->data;
      gint index = find_project (other, marker_project_get_uri (project));
      if (index >= 0)
        {
          g_autoptr (MarkerProject) shared = project_at (other, index);
          g_set_object (&project, shared);
          shared_model = TRUE;
          break;
        }
    }
  if (!shared_model)
    marker_session_restore_project (project);
  g_signal_connect_object (project, "notify", G_CALLBACK (project_changed_cb), self, 0);
  g_signal_connect_object (project, "styles-changed", G_CALLBACK (project_styles_changed_cb), self, 0);
  g_list_store_append (self->projects, project);
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->documents)); i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      assign_editor_project (self, editor);
    }
  update_loose_files (self);
  gtk_single_selection_set_selected (self->project_selection,
                                     g_list_model_get_n_items (G_LIST_MODEL (self->projects)) - 1);
  if (adw_overlay_split_view_get_collapsed (self->split_view))
    adw_overlay_split_view_set_show_sidebar (self->split_view, TRUE);
}


void
marker_window_new_editor (MarkerWindow *self)
{
  add_editor (self, marker_editor_new ());
}

void
marker_window_new_editor_from_file (MarkerWindow *self,
                                    GFile        *file)
{
  gint existing = find_document (self, file);
  if (existing >= 0)
    {
      gtk_single_selection_set_selected (self->document_selection, existing);
      return;
    }
  add_editor (self, marker_editor_new_from_file (file));
}

void
marker_window_restore_saved_document (MarkerWindow *self,
                                      GFile        *file)
{
  if (g_file_query_exists (file, NULL))
    marker_window_new_editor_from_file (self, file);
}

MarkerEditor *
marker_window_get_active_editor (MarkerWindow *self)
{
  guint selected;
  g_return_val_if_fail (MARKER_IS_WINDOW (self), NULL);
  selected = gtk_single_selection_get_selected (self->document_selection);
  if (selected == GTK_INVALID_LIST_POSITION)
    return NULL;
  return MARKER_EDITOR (gtk_single_selection_get_selected_item (self->document_selection));
}




void
marker_window_fullscreen (MarkerWindow *self)
{
  gtk_window_fullscreen (GTK_WINDOW (self));
  self->fullscreen = TRUE;
}

void
marker_window_unfullscreen (MarkerWindow *self)
{
  gtk_window_unfullscreen (GTK_WINDOW (self));
  self->fullscreen = FALSE;
}

void
marker_window_toggle_fullscreen (MarkerWindow *self)
{
  if (self->fullscreen)
    marker_window_unfullscreen (self);
  else
    marker_window_fullscreen (self);
}


static void
remove_editor (MarkerWindow *self,
               MarkerEditor *editor)
{
  marker_editor_closing (editor);
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));
  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) candidate = document_at (self, i);
      if (candidate == editor)
        {
          gtk_stack_remove (GTK_STACK (self->editor_stack), GTK_WIDGET (editor));
          g_list_store_remove (self->documents, i);
          break;
        }
    }
  if (g_list_model_get_n_items (G_LIST_MODEL (self->documents)) == 0)
    gtk_stack_set_visible_child (GTK_STACK (self->editor_stack), self->empty_state);
  update_loose_files (self);
  update_title (self);
}

static void
save_all_and_close (MarkerWindow *self)
{
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));

  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      if (!marker_editor_has_unsaved_changes (editor))
        continue;
      if (marker_editor_get_file (editor) == NULL)
        {
          select_document (self, editor);
          save_editor_as (self, editor, FALSE, TRUE, NULL);
          return;
        }
      if (!marker_editor_save_file (editor))
        {
          select_document (self, editor);
          show_error (self, _("Unable to save file"),
                      _("The document could not be written. The window remains open."));
          return;
        }
    }

  marker_save_session ();
  self->closing = TRUE;
  gtk_window_destroy (GTK_WINDOW (self));
}

static void
close_document_response_cb (AdwAlertDialog *dialog,
                            const char     *response,
                            MarkerWindow   *self)
{
  MarkerEditor *editor = g_object_get_data (G_OBJECT (dialog), "editor");
  if (editor == NULL)
    return;
  if (g_str_equal (response, "discard"))
    remove_editor (self, editor);
  else if (g_str_equal (response, "save"))
    {
      if (marker_editor_get_file (editor) == NULL)
        {
          select_document (self, editor);
          save_editor_as (self, editor, TRUE, FALSE, NULL);
        }
      else
        {
          if (marker_editor_save_file (editor))
            remove_editor (self, editor);
          else
            show_error (self, _("Unable to save file"),
                        _("The document could not be written and remains open."));
        }
    }
}

void
marker_window_close_current_document (MarkerWindow *self)
{
  MarkerEditor *editor = marker_window_get_active_editor (self);
  if (editor == NULL)
    return;
  if (!marker_editor_has_unsaved_changes (editor))
    {
      remove_editor (self, editor);
      return;
    }

  AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (
    _("Save changes?"), _("Unsaved changes will be lost if this document is closed.")));
  adw_alert_dialog_add_responses (dialog,
                                  "cancel", _("Cancel"),
                                  "discard", _("Discard"),
                                  "save", _("Save"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (dialog, "discard", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_response_appearance (dialog, "save", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (dialog, "save");
  adw_alert_dialog_set_close_response (dialog, "cancel");
  g_object_set_data_full (G_OBJECT (dialog), "editor", g_object_ref (editor), g_object_unref);
  g_signal_connect (dialog, "response", G_CALLBACK (close_document_response_cb), self);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
}

gboolean
marker_window_try_close (MarkerWindow *self)
{
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));
  guint unsaved = 0;

  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      if (marker_editor_has_unsaved_changes (editor))
        unsaved++;
    }
  if (unsaved == 0)
    return TRUE;

  g_autofree char *body = g_strdup_printf (
    ngettext ("One document has unsaved changes.",
              "%u documents have unsaved changes.", unsaved), unsaved);
  AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (_("Close window?"), body));
  adw_alert_dialog_add_responses (dialog,
                                  "cancel", _("Cancel"),
                                  "discard", _("Discard All"),
                                  "save", _("Save"),
                                  NULL);
  adw_alert_dialog_set_response_appearance (dialog, "discard", ADW_RESPONSE_DESTRUCTIVE);
  adw_alert_dialog_set_response_appearance (dialog, "save", ADW_RESPONSE_SUGGESTED);
  adw_alert_dialog_set_default_response (dialog, "save");
  adw_alert_dialog_set_close_response (dialog, "cancel");
  g_signal_connect (dialog, "response", G_CALLBACK (close_window_response_cb), self);
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (self));
  return FALSE;
}





void
marker_window_apply_prefs (MarkerWindow *self)
{
  guint count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));
  for (guint i = 0; i < count; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      marker_editor_apply_prefs (editor);
    }
}



MarkerSessionWindow *
marker_window_capture_session (MarkerWindow *self)
{
  MarkerSessionWindow *session = marker_session_window_new ();
  guint project_count = g_list_model_get_n_items (G_LIST_MODEL (self->projects));
  guint document_count = g_list_model_get_n_items (G_LIST_MODEL (self->documents));

  for (guint i = 0; i < project_count; i++)
    {
      g_autoptr (MarkerProject) project = project_at (self, i);
      g_ptr_array_add (session->projects, g_strdup (marker_project_get_uri (project)));
      marker_session_store_project (project, NULL);
    }
  for (guint i = 0; i < document_count; i++)
    {
      g_autoptr (MarkerEditor) editor = document_at (self, i);
      GFile *file = marker_editor_get_file (editor);
      if (file != NULL)
        {
          g_autofree char *uri = g_file_get_uri (file);
          g_ptr_array_add (session->documents, g_steal_pointer (&uri));
        }
    }

  session->has_navigation = TRUE;
  session->pages_visible = self->pages_visible;
  session->outline_visible = self->outline_visible;
  session->files_mode = marker_page_sidebar_get_files (self->project_sidebar);
  session->active_project = gtk_single_selection_get_selected (self->project_selection);
  MarkerEditor *active = marker_window_get_active_editor (self);
  if (active != NULL && marker_editor_get_file (active) != NULL)
    session->active_document = g_file_get_uri (marker_editor_get_file (active));
  return session;
}

void
marker_window_set_active_project_index (MarkerWindow *self,
                                        guint         index)
{
  if (index == GTK_INVALID_LIST_POSITION || index < g_list_model_get_n_items (G_LIST_MODEL (self->projects)))
    gtk_single_selection_set_selected (self->project_selection, index);
}

void
marker_window_set_active_document_uri (MarkerWindow *self,
                                       const char   *uri)
{
  if (uri == NULL)
    return;
  g_autoptr (GFile) file = g_file_new_for_uri (uri);
  gint position = find_document (self, file);
  if (position >= 0)
    gtk_single_selection_set_selected (self->document_selection, position);
}

void
marker_window_restore_navigation (MarkerWindow *self, MarkerSessionWindow *session)
{
  if (session->has_navigation)
    {
      self->pages_visible = session->pages_visible;
      self->outline_visible = session->outline_visible;
      marker_page_sidebar_set_files (self->project_sidebar, session->files_mode);
    }
  sync_navigation (self);
}
