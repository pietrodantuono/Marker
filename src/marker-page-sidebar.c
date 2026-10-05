/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-page-sidebar.h"
#include "marker-workspace.h"
#include "marker-prefs.h"
#include <glib/gi18n.h>

struct _MarkerPageSidebar
{
  GtkBox parent_instance;
  GListModel *documents;
  MarkerProject *project;
  MarkerEditor *active;
  GPtrArray *disk_items;
  GListStore *pages;
  GtkSingleSelection *selection;
  GtkListView *view;
  GtkStack *stack;
  GtkLabel *title;
  GtkMenuButton *title_menu;
  GtkSearchBar *search_bar;
  GtkSearchEntry *search;
  GtkToggleButton *files;
  GtkToggleButton *pages_button;
  GCancellable *scan;
  guint generation;
  guint search_source;
  GtkCssProvider *provider;
  GdkDisplay *display;
  char *css_class;
};
G_DEFINE_TYPE (MarkerPageSidebar, marker_page_sidebar, GTK_TYPE_BOX)

enum { OPEN_FILE, SELECT_PAGE, CLOSE_PAGE, LAST_SIGNAL };
static guint signals[LAST_SIGNAL];
static void rebuild (MarkerPageSidebar *self);

static gboolean
search_rebuild_cb (gpointer data)
{
  MarkerPageSidebar *self = data;
  self->search_source = 0;
  rebuild (self);
  return G_SOURCE_REMOVE;
}

static void
page_title_changed_cb (MarkerEditor *editor, MarkerPageSidebar *self)
{
  if (self->documents == NULL)
    return;
  if (*gtk_editable_get_text (GTK_EDITABLE (self->search)) != '\0')
    {
      g_clear_handle_id (&self->search_source, g_source_remove);
      self->search_source = g_timeout_add (180, search_rebuild_cb, self);
    }
}

static GFile *
row_file (GObject *item)
{
  if (GTK_IS_TREE_LIST_ROW (item))
    item = gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (item));
  return MARKER_IS_EDITOR (item) ? marker_editor_get_file (MARKER_EDITOR (item))
                                 : marker_workspace_item_get_file (MARKER_WORKSPACE_ITEM (item));
}

static MarkerEditor *
find_editor (MarkerPageSidebar *self, GFile *file)
{
  if (file == NULL)
    return NULL;
  for (guint i = 0; i < g_list_model_get_n_items (self->documents); i++)
    {
      g_autoptr (MarkerEditor) editor = g_list_model_get_item (self->documents, i);
      GFile *candidate = marker_editor_get_file (editor);
      if (candidate != NULL && g_file_equal (candidate, file))
        return editor; /* The document model owns it. */
    }
  return NULL;
}

static gboolean
belongs (MarkerPageSidebar *self, MarkerEditor *editor)
{
  return marker_editor_get_project (editor) == self->project;
}

static gboolean
matches (MarkerPageSidebar *self, GObject *item)
{
  const char *query = gtk_editable_get_text (GTK_EDITABLE (self->search));
  if (*query == '\0')
    return TRUE;
  g_autofree char *folded = g_utf8_casefold (query, -1);
  g_autofree char *title = MARKER_IS_EDITOR (item)
    ? marker_editor_get_page_title (MARKER_EDITOR (item))
    : g_strdup (marker_workspace_item_get_title (MARKER_WORKSPACE_ITEM (item)));
  GFile *file = row_file (item);
  g_autofree char *path = file == NULL ? g_strdup ("") : self->project == NULL
    ? g_file_get_parse_name (file) : g_file_get_relative_path (marker_project_get_root (self->project), file);
  g_autofree char *text = g_strdup_printf ("%s %s", title, path != NULL ? path : "");
  g_autofree char *haystack = g_utf8_casefold (text, -1);
  return strstr (haystack, folded) != NULL;
}

typedef struct
{
  MarkerPageSidebar *sidebar; /* Child row cannot outlive its sidebar. */
  GtkTreeExpander *expander;
  GtkLabel *title;
  GtkLabel *subtitle;
  GtkWidget *dirty;
  GtkWidget *close;
  GtkImage *icon;
  GtkPopover *menu;
  GFile *file;
  MarkerEditor *editor;
} PageRow;

static void
row_update (MarkerEditor *editor, PageRow *row)
{
  if (editor == NULL)
    return;
  g_autofree char *title = marker_editor_get_page_title (editor);
  gtk_label_set_text (row->title, title);
  gtk_widget_set_visible (row->dirty, marker_editor_has_unsaved_changes (editor));
}

static void
row_disconnect (PageRow *row)
{
  if (row->editor != NULL)
    g_signal_handlers_disconnect_by_data (row->editor, row);
  g_clear_object (&row->editor);
  g_clear_object (&row->file);
  gtk_popover_popdown (row->menu);
}

static void
row_free (PageRow *row)
{
  row_disconnect (row);
  if (gtk_widget_get_parent (GTK_WIDGET (row->menu)) != NULL)
    gtk_widget_unparent (GTK_WIDGET (row->menu));
  g_clear_object (&row->menu);
  g_free (row);
}

static void
close_cb (GtkButton *button, PageRow *row)
{
  if (row->editor != NULL)
    g_signal_emit (row->sidebar, signals[CLOSE_PAGE], 0, row->editor);
}

static void
open_row (PageRow *row, gboolean external)
{
  if (!external && row->editor != NULL)
    g_signal_emit (row->sidebar, signals[SELECT_PAGE], 0, row->editor);
  else if (row->file != NULL)
    g_signal_emit (row->sidebar, signals[OPEN_FILE], 0, row->file, external);
}

static void
context_clicked_cb (GtkButton *button, PageRow *row)
{
  guint action = GPOINTER_TO_UINT (g_object_get_data (G_OBJECT (button), "action"));
  /* Opening a page may replace the list model and destroy this row. */
  gtk_popover_popdown (row->menu);
  if (action == 2 && row->file != NULL)
    {
      g_autofree char *path = row->sidebar->project != NULL
        ? g_file_get_relative_path (marker_project_get_root (row->sidebar->project), row->file) : NULL;
      if (path == NULL)
        path = g_file_get_parse_name (row->file);
      gdk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (button)), path);
    }
  else
    open_row (row, action == 1);
}

static void
context_pressed_cb (GtkGestureClick *gesture, int n_press, double x, double y, PageRow *row)
{
  if (row->file == NULL)
    return;
  GdkRectangle rectangle = { x, y, 1, 1 };
  gtk_popover_set_pointing_to (row->menu, &rectangle);
  gtk_popover_popup (row->menu);
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
setup_cb (GtkSignalListItemFactory *factory, GtkListItem *item, MarkerPageSidebar *self)
{
  PageRow *row = g_new0 (PageRow, 1);
  row->sidebar = self;
  row->expander = GTK_TREE_EXPANDER (gtk_tree_expander_new ());
  row->title = GTK_LABEL (gtk_label_new (NULL));
  row->subtitle = GTK_LABEL (gtk_label_new (NULL));
  row->dirty = gtk_label_new ("•");
  row->close = gtk_button_new_from_icon_name ("window-close-symbolic");
  row->icon = GTK_IMAGE (gtk_image_new ());
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
  GtkWidget *labels = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
  gtk_label_set_xalign (row->title, 0);
  gtk_label_set_xalign (row->subtitle, 0);
  gtk_label_set_ellipsize (row->title, PANGO_ELLIPSIZE_END);
  gtk_label_set_ellipsize (row->subtitle, PANGO_ELLIPSIZE_MIDDLE);
  gtk_widget_add_css_class (GTK_WIDGET (row->title), "heading");
  gtk_widget_add_css_class (GTK_WIDGET (row->subtitle), "marker-secondary");
  gtk_widget_add_css_class (row->dirty, "marker-dirty");
  gtk_widget_add_css_class (row->close, "flat");
  gtk_widget_add_css_class (box, "marker-page-row");
  gtk_widget_set_tooltip_text (row->close, _("Close Page"));
  gtk_widget_set_hexpand (labels, TRUE);
  gtk_box_append (GTK_BOX (labels), GTK_WIDGET (row->title));
  gtk_box_append (GTK_BOX (labels), GTK_WIDGET (row->subtitle));
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (row->icon));
  gtk_box_append (GTK_BOX (box), labels);
  gtk_box_append (GTK_BOX (box), row->dirty);
  gtk_box_append (GTK_BOX (box), row->close);
  gtk_tree_expander_set_child (row->expander, box);
  gtk_tree_expander_set_indent_for_icon (row->expander, FALSE);
  row->menu = g_object_ref_sink (GTK_POPOVER (gtk_popover_new ()));
  GtkWidget *menu_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  const char *labels_text[] = { _("Open"), _("Open Externally"), _("Copy Relative Path") };
  for (guint i = 0; i < G_N_ELEMENTS (labels_text); i++)
    {
      GtkWidget *button = gtk_button_new_with_label (labels_text[i]);
      gtk_widget_add_css_class (button, "flat");
      g_object_set_data (G_OBJECT (button), "action", GUINT_TO_POINTER (i));
      g_signal_connect (button, "clicked", G_CALLBACK (context_clicked_cb), row);
      gtk_box_append (GTK_BOX (menu_box), button);
    }
  gtk_popover_set_child (row->menu, menu_box);
  GtkGesture *gesture = gtk_gesture_click_new ();
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (context_pressed_cb), row);
  gtk_widget_add_controller (GTK_WIDGET (row->expander), GTK_EVENT_CONTROLLER (gesture));
  g_signal_connect (row->close, "clicked", G_CALLBACK (close_cb), row);
  g_object_set_data_full (G_OBJECT (item), "row", row, (GDestroyNotify) row_free);
  gtk_list_item_set_child (item, GTK_WIDGET (row->expander));
}

static void
bind_cb (GtkSignalListItemFactory *factory, GtkListItem *list_item, MarkerPageSidebar *self)
{
  PageRow *row = g_object_get_data (G_OBJECT (list_item), "row");
  GObject *item = gtk_list_item_get_item (list_item);
  gboolean tree = GTK_IS_TREE_LIST_ROW (item);
  row_disconnect (row);
  if (gtk_widget_get_parent (GTK_WIDGET (row->menu)) == NULL)
    gtk_widget_set_parent (GTK_WIDGET (row->menu), GTK_WIDGET (row->expander));
  gtk_tree_expander_set_list_row (row->expander, tree ? GTK_TREE_LIST_ROW (item) : NULL);
  if (tree)
    item = gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (item));
  gboolean editor_item = MARKER_IS_EDITOR (item);
  GFile *file = row_file (item);
  g_set_object (&row->file, file);
  MarkerEditor *editor = editor_item ? MARKER_EDITOR (item) : find_editor (self, file);
  g_set_object (&row->editor, editor);
  if (editor != NULL)
    {
      row_update (editor, row);
      g_signal_connect (editor, "title-changed", G_CALLBACK (row_update), row);
    }
  else
    gtk_label_set_text (row->title, marker_workspace_item_get_title (MARKER_WORKSPACE_ITEM (item)));
  g_autofree char *path = file == NULL ? g_strdup (_("Unsaved Page")) : self->project == NULL
    ? g_file_get_parse_name (file) : g_file_get_relative_path (marker_project_get_root (self->project), file);
  gtk_label_set_text (row->subtitle, path != NULL ? path : "");
  gtk_widget_set_visible (row->close, editor != NULL);
  gtk_widget_set_visible (row->dirty, editor != NULL && marker_editor_has_unsaved_changes (editor));
  gtk_widget_set_visible (GTK_WIDGET (row->icon), marker_page_sidebar_get_files (self));
  gtk_image_set_from_icon_name (row->icon, editor_item ? "text-x-generic-symbolic"
    : marker_workspace_item_get_icon_name (MARKER_WORKSPACE_ITEM (item)));
}

static void
unbind_cb (GtkSignalListItemFactory *factory, GtkListItem *item, gpointer data)
{
  PageRow *row = g_object_get_data (G_OBJECT (item), "row");
  row_disconnect (row);
  if (gtk_widget_get_parent (GTK_WIDGET (row->menu)) != NULL)
    gtk_widget_unparent (GTK_WIDGET (row->menu));
  gtk_tree_expander_set_list_row (row->expander, NULL);
}

static void
activated_cb (GtkListView *view, guint position, MarkerPageSidebar *self)
{
  g_autoptr (GObject) object = g_list_model_get_item (G_LIST_MODEL (self->selection), position);
  if (GTK_IS_TREE_LIST_ROW (object))
    {
      MarkerWorkspaceItem *item = gtk_tree_list_row_get_item (GTK_TREE_LIST_ROW (object));
      if (marker_workspace_item_get_directory (item))
        {
          gtk_tree_list_row_set_expanded (GTK_TREE_LIST_ROW (object), !gtk_tree_list_row_get_expanded (GTK_TREE_LIST_ROW (object)));
          return;
        }
    }
  GFile *file = row_file (object);
  if (MARKER_IS_EDITOR (object))
    g_signal_emit (self, signals[SELECT_PAGE], 0, object);
  else
    g_signal_emit (self, signals[OPEN_FILE], 0, file, FALSE);
}

static GListModel *
child_model (gpointer item, gpointer data)
{
  return marker_workspace_item_get_children (item);
}

typedef struct
{
  gboolean modified;
  GHashTable *disk_by_file; /* Borrowed items during a single sort. */
} PageSort;

static guint64
page_modified (PageSort *sort, GObject *page)
{
  if (MARKER_IS_WORKSPACE_ITEM (page))
    return marker_workspace_item_get_modified (MARKER_WORKSPACE_ITEM (page));
  GFile *file = row_file (page);
  MarkerWorkspaceItem *item = file != NULL ? g_hash_table_lookup (sort->disk_by_file, file) : NULL;
  return item != NULL ? marker_workspace_item_get_modified (item) : 0;
}

static gint
compare_pages (gconstpointer left, gconstpointer right, gpointer data)
{
  PageSort *sort = data;
  GObject *a = (GObject *) left;
  GObject *b = (GObject *) right;
  GFile *a_file = row_file (a);
  GFile *b_file = row_file (b);
  if ((a_file == NULL) != (b_file == NULL))
    return a_file == NULL ? -1 : 1;
  if (sort->modified)
    {
      guint64 a_time = page_modified (sort, a);
      guint64 b_time = page_modified (sort, b);
      if (a_time != b_time)
        return a_time > b_time ? -1 : 1;
    }
  g_autofree char *a_name = a_file != NULL ? g_file_get_basename (a_file) : marker_editor_get_page_title (MARKER_EDITOR (a));
  g_autofree char *b_name = b_file != NULL ? g_file_get_basename (b_file) : marker_editor_get_page_title (MARKER_EDITOR (b));
  return g_utf8_collate (a_name, b_name);
}

static void
rebuild (MarkerPageSidebar *self)
{
  if (self->documents == NULL)
    return;
  gboolean files = marker_page_sidebar_get_files (self);
  gboolean searching = *gtk_editable_get_text (GTK_EDITABLE (self->search)) != '\0';
  g_autoptr (GListModel) model = NULL;
  if (files && !searching && self->project != NULL && !marker_project_get_missing (self->project))
    {
      g_autoptr (MarkerWorkspaceItem) root = marker_workspace_item_new_root (marker_project_get_root (self->project));
      g_autofree char *sort = g_settings_get_string (prefs.window_settings, "sidebar-sort");
      marker_workspace_item_set_sort (root, sort);
      model = G_LIST_MODEL (gtk_tree_list_model_new (g_object_ref (marker_workspace_item_get_children (root)),
                                                    FALSE, FALSE, child_model, NULL, NULL));
      /* The task owns its cancellable/store; keep the root until this model dies. */
      g_object_set_data_full (G_OBJECT (model), "root", g_object_ref (root), g_object_unref);
    }
  else
    {
      g_list_store_remove_all (self->pages);
      for (guint i = 0; i < g_list_model_get_n_items (self->documents); i++)
        {
          g_autoptr (MarkerEditor) editor = g_list_model_get_item (self->documents, i);
          GFile *file = marker_editor_get_file (editor);
          g_autofree char *name = file != NULL ? g_file_get_basename (file) : NULL;
          if (belongs (self, editor) && (files || name == NULL || !g_str_has_suffix (name, ".css")) && matches (self, G_OBJECT (editor)))
            g_list_store_append (self->pages, editor);
        }
      for (guint i = 0; self->disk_items != NULL && i < self->disk_items->len; i++)
        {
          MarkerWorkspaceItem *item = g_ptr_array_index (self->disk_items, i);
          if ((files || marker_workspace_item_get_markdown (item)) &&
              find_editor (self, marker_workspace_item_get_file (item)) == NULL && matches (self, G_OBJECT (item)))
            g_list_store_append (self->pages, item);
        }
      g_autofree char *order = g_settings_get_string (prefs.window_settings, "sidebar-sort");
      g_autoptr (GHashTable) by_file = g_hash_table_new ((GHashFunc) g_file_hash, (GEqualFunc) g_file_equal);
      for (guint i = 0; self->disk_items != NULL && i < self->disk_items->len; i++)
        {
          MarkerWorkspaceItem *item = g_ptr_array_index (self->disk_items, i);
          g_hash_table_insert (by_file, marker_workspace_item_get_file (item), item);
        }
      PageSort sort = { g_str_equal (order, "modified"), by_file };
      g_list_store_sort (self->pages, compare_pages, &sort);
      model = G_LIST_MODEL (g_object_ref (self->pages));
    }
  g_clear_object (&self->selection);
  self->selection = gtk_single_selection_new (g_steal_pointer (&model));
  gtk_single_selection_set_autoselect (self->selection, FALSE);
  gtk_single_selection_set_can_unselect (self->selection, TRUE);
  gtk_single_selection_set_selected (self->selection, GTK_INVALID_LIST_POSITION);
  gtk_list_view_set_model (self->view, GTK_SELECTION_MODEL (self->selection));
  marker_page_sidebar_set_active (self, self->active);
  gtk_stack_set_visible_child_name (self->stack, self->project != NULL && marker_project_get_missing (self->project) ? "missing" : "list");
}

typedef struct { GWeakRef sidebar; guint generation; } ScanRequest;

static void
scan_done_cb (GObject *source, GAsyncResult *result, gpointer data)
{
  ScanRequest *request = data;
  g_autoptr (MarkerPageSidebar) self = g_weak_ref_get (&request->sidebar);
  g_autoptr (GError) error = NULL;
  g_autoptr (GPtrArray) items = marker_workspace_scan_finish (result, &error);
  if (self != NULL && self->generation == request->generation)
    {
      g_clear_pointer (&self->disk_items, g_ptr_array_unref);
      self->disk_items = g_steal_pointer (&items);
      rebuild (self);
    }
  g_weak_ref_clear (&request->sidebar);
  g_free (request);
}

void
marker_page_sidebar_refresh (MarkerPageSidebar *self)
{
  self->generation++;
  if (self->scan != NULL)
    g_cancellable_cancel (self->scan);
  g_clear_object (&self->scan);
  g_clear_pointer (&self->disk_items, g_ptr_array_unref);
  if (self->project != NULL && !marker_project_get_missing (self->project))
    {
      self->scan = g_cancellable_new ();
      ScanRequest *request = g_new0 (ScanRequest, 1);
      g_weak_ref_init (&request->sidebar, self);
      request->generation = self->generation;
      marker_workspace_scan_async (marker_project_get_root (self->project), self->scan, scan_done_cb, request);
    }
  rebuild (self);
}

static void
appearance_cb (MarkerProject *project, GParamSpec *pspec, MarkerPageSidebar *self)
{
  gtk_label_set_text (self->title, marker_project_get_name (project));
  g_autofree char *css = g_strdup_printf (".%s row:selected {background: alpha(%s,.22); color: @window_fg_color;}",
                                         self->css_class, marker_project_get_color (project));
  gtk_css_provider_load_from_string (self->provider, css);
  if (pspec != NULL && g_str_equal (pspec->name, "root"))
    marker_page_sidebar_refresh (self);
}

void
marker_page_sidebar_set_project (MarkerPageSidebar *self, MarkerProject *project)
{
  if (self->project == project)
    return;
  if (self->project != NULL)
    g_signal_handlers_disconnect_by_data (self->project, self);
  g_set_object (&self->project, project);
  gtk_widget_set_sensitive (GTK_WIDGET (self->title_menu), project != NULL);
  if (project != NULL)
    {
      g_signal_connect_object (project, "notify", G_CALLBACK (appearance_cb), self, 0);
      appearance_cb (project, NULL, self);
    }
  else
    {
      gtk_label_set_text (self->title, _("Open Files"));
      gtk_css_provider_load_from_string (self->provider, "");
    }
  gtk_editable_set_text (GTK_EDITABLE (self->search), "");
  marker_page_sidebar_refresh (self);
}

void
marker_page_sidebar_set_active (MarkerPageSidebar *self, MarkerEditor *editor)
{
  g_set_object (&self->active, editor);
  if (self->selection == NULL)
    return;
  guint selected = GTK_INVALID_LIST_POSITION;
  GFile *file = editor != NULL ? marker_editor_get_file (editor) : NULL;
  for (guint i = 0; i < g_list_model_get_n_items (G_LIST_MODEL (self->selection)); i++)
    {
      g_autoptr (GObject) item = g_list_model_get_item (G_LIST_MODEL (self->selection), i);
      GFile *candidate = row_file (item);
      if (item == (GObject *) editor || (file != NULL && candidate != NULL && g_file_equal (file, candidate)))
        { selected = i; break; }
    }
  gtk_single_selection_set_selected (self->selection, selected);
}

gboolean
marker_page_sidebar_get_files (MarkerPageSidebar *self)
{
  return gtk_toggle_button_get_active (self->files);
}

void
marker_page_sidebar_set_files (MarkerPageSidebar *self, gboolean files)
{
  gtk_toggle_button_set_active (files ? self->files : self->pages_button, TRUE);
}

static void
changed_cb (GtkWidget *widget, MarkerPageSidebar *self)
{
  rebuild (self);
}

static void
documents_changed_cb (GListModel *model, guint position, guint removed, guint added, MarkerPageSidebar *self)
{
  for (guint i = position; i < position + added; i++)
    {
      g_autoptr (MarkerEditor) editor = g_list_model_get_item (model, i);
      g_signal_handlers_disconnect_by_func (editor, page_title_changed_cb, self);
      g_signal_connect_object (editor, "title-changed", G_CALLBACK (page_title_changed_cb), self, 0);
    }
  rebuild (self);
}

static void
marker_page_sidebar_dispose (GObject *object)
{
  MarkerPageSidebar *self = MARKER_PAGE_SIDEBAR (object);
  self->generation++;
  g_clear_handle_id (&self->search_source, g_source_remove);
  if (self->scan != NULL)
    g_cancellable_cancel (self->scan);
  if (self->project != NULL)
    g_signal_handlers_disconnect_by_data (self->project, self);
  g_clear_object (&self->scan);
  g_clear_object (&self->project);
  if (self->documents != NULL)
    g_signal_handlers_disconnect_by_data (self->documents, self);
  g_clear_object (&self->documents);
  g_clear_object (&self->active);
  g_clear_object (&self->pages);
  g_clear_object (&self->selection);
  g_clear_pointer (&self->disk_items, g_ptr_array_unref);
  if (self->provider != NULL)
    gtk_style_context_remove_provider_for_display (self->display, GTK_STYLE_PROVIDER (self->provider));
  g_clear_object (&self->provider);
  g_clear_object (&self->display);
  G_OBJECT_CLASS (marker_page_sidebar_parent_class)->dispose (object);
}

static void
marker_page_sidebar_finalize (GObject *object)
{
  g_free (MARKER_PAGE_SIDEBAR (object)->css_class);
  G_OBJECT_CLASS (marker_page_sidebar_parent_class)->finalize (object);
}

static void
marker_page_sidebar_class_init (MarkerPageSidebarClass *klass)
{
  G_OBJECT_CLASS (klass)->dispose = marker_page_sidebar_dispose;
  G_OBJECT_CLASS (klass)->finalize = marker_page_sidebar_finalize;
  signals[OPEN_FILE] = g_signal_new ("open-file", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                                    0, NULL, NULL, NULL, G_TYPE_NONE, 2, G_TYPE_FILE, G_TYPE_BOOLEAN);
  signals[SELECT_PAGE] = g_signal_new ("select-page", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                                      0, NULL, NULL, NULL, G_TYPE_NONE, 1, MARKER_TYPE_EDITOR);
  signals[CLOSE_PAGE] = g_signal_new ("close-page", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                                     0, NULL, NULL, NULL, G_TYPE_NONE, 1, MARKER_TYPE_EDITOR);
}

static void
marker_page_sidebar_init (MarkerPageSidebar *self)
{
  static guint next_class;
  self->display = g_object_ref (gtk_widget_get_display (GTK_WIDGET (self)));
  self->provider = gtk_css_provider_new ();
  self->css_class = g_strdup_printf ("marker-pages-%u", next_class++);
  gtk_widget_add_css_class (GTK_WIDGET (self), self->css_class);
  gtk_widget_add_css_class (GTK_WIDGET (self), "marker-sidebar");
  gtk_style_context_add_provider_for_display (self->display, GTK_STYLE_PROVIDER (self->provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  self->pages = g_list_store_new (G_TYPE_OBJECT);
  GtkWidget *header = adw_header_bar_new ();
  self->title = GTK_LABEL (gtk_label_new (_("Open Files")));
  gtk_label_set_ellipsize (self->title, PANGO_ELLIPSIZE_END);
  gtk_widget_add_css_class (GTK_WIDGET (self->title), "heading");
  self->title_menu = GTK_MENU_BUTTON (gtk_menu_button_new ());
  gtk_menu_button_set_child (self->title_menu, GTK_WIDGET (self->title));
  gtk_widget_add_css_class (GTK_WIDGET (self->title_menu), "flat");
  gtk_widget_set_sensitive (GTK_WIDGET (self->title_menu), FALSE);
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->title_menu), _("Notebook Navigation Options"));
  g_autoptr (GMenu) menu = g_menu_new ();
  g_menu_append (menu, _("Refresh"), "win.sidebar-refresh");
  g_autoptr (GMenu) sort = g_menu_new ();
  GMenuItem *name_sort = g_menu_item_new (_("Name"), NULL);
  g_menu_item_set_action_and_target (name_sort, "win.sort", "s", "name");
  g_menu_append_item (sort, name_sort);
  g_object_unref (name_sort);
  GMenuItem *modified_sort = g_menu_item_new (_("Last Modified"), NULL);
  g_menu_item_set_action_and_target (modified_sort, "win.sort", "s", "modified");
  g_menu_append_item (sort, modified_sort);
  g_object_unref (modified_sort);
  g_menu_append_submenu (menu, _("Sort By"), G_MENU_MODEL (sort));
  g_menu_append (menu, _("Edit Notebook Styles"), "win.notebook-styles");
  gtk_menu_button_set_menu_model (self->title_menu, G_MENU_MODEL (menu));
  GtkWidget *new_page = gtk_button_new_from_icon_name ("list-add-symbolic");
  GtkWidget *search = gtk_toggle_button_new ();
  gtk_button_set_icon_name (GTK_BUTTON (search), "edit-find-symbolic");
  gtk_widget_set_tooltip_text (new_page, _("New Page"));
  gtk_widget_set_tooltip_text (search, _("Search Pages or Files"));
  gtk_actionable_set_action_name (GTK_ACTIONABLE (new_page), "win.new-document");
  adw_header_bar_set_show_start_title_buttons (ADW_HEADER_BAR (header), FALSE);
  adw_header_bar_set_show_end_title_buttons (ADW_HEADER_BAR (header), FALSE);
  adw_header_bar_set_title_widget (ADW_HEADER_BAR (header), GTK_WIDGET (self->title_menu));
  adw_header_bar_pack_start (ADW_HEADER_BAR (header), new_page);
  adw_header_bar_pack_end (ADW_HEADER_BAR (header), search);
  self->search_bar = GTK_SEARCH_BAR (gtk_search_bar_new ());
  self->search = GTK_SEARCH_ENTRY (gtk_search_entry_new ());
  gtk_search_bar_set_child (self->search_bar, GTK_WIDGET (self->search));
  gtk_search_bar_connect_entry (self->search_bar, GTK_EDITABLE (self->search));
  g_object_bind_property (search, "active", self->search_bar, "search-mode-enabled", G_BINDING_BIDIRECTIONAL);
  GtkWidget *switcher = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  GtkWidget *pages = gtk_toggle_button_new_with_label (_("Pages"));
  self->pages_button = GTK_TOGGLE_BUTTON (pages);
  self->files = GTK_TOGGLE_BUTTON (gtk_toggle_button_new_with_label (_("Files")));
  gtk_toggle_button_set_group (self->files, GTK_TOGGLE_BUTTON (pages));
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (pages), TRUE);
  gtk_widget_set_hexpand (pages, TRUE);
  gtk_widget_set_hexpand (GTK_WIDGET (self->files), TRUE);
  gtk_widget_add_css_class (switcher, "linked");
  gtk_widget_add_css_class (switcher, "marker-page-switch");
  gtk_box_append (GTK_BOX (switcher), pages);
  gtk_box_append (GTK_BOX (switcher), GTK_WIDGET (self->files));
  GtkListItemFactory *factory = gtk_signal_list_item_factory_new ();
  g_signal_connect (factory, "setup", G_CALLBACK (setup_cb), self);
  g_signal_connect (factory, "bind", G_CALLBACK (bind_cb), self);
  g_signal_connect (factory, "unbind", G_CALLBACK (unbind_cb), self);
  self->view = GTK_LIST_VIEW (gtk_list_view_new (NULL, factory));
  gtk_list_view_set_single_click_activate (self->view, TRUE);
  GtkWidget *scroller = gtk_scrolled_window_new ();
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (scroller), GTK_WIDGET (self->view));
  self->stack = GTK_STACK (gtk_stack_new ());
  gtk_stack_add_named (self->stack, scroller, "list");
  GtkWidget *missing = adw_status_page_new ();
  adw_status_page_set_title (ADW_STATUS_PAGE (missing), _("Notebook Unavailable"));
  adw_status_page_set_description (ADW_STATUS_PAGE (missing), _("Locate the folder using its notebook menu."));
  adw_status_page_set_icon_name (ADW_STATUS_PAGE (missing), "folder-missing-symbolic");
  gtk_stack_add_named (self->stack, missing, "missing");
  gtk_widget_set_vexpand (GTK_WIDGET (self->stack), TRUE);
  gtk_box_append (GTK_BOX (self), header);
  gtk_box_append (GTK_BOX (self), switcher);
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->search_bar));
  gtk_box_append (GTK_BOX (self), GTK_WIDGET (self->stack));
  g_signal_connect_object (self->view, "activate", G_CALLBACK (activated_cb), self, 0);
  g_signal_connect_object (self->search, "search-changed", G_CALLBACK (changed_cb), self, 0);
  g_signal_connect_object (self->files, "toggled", G_CALLBACK (changed_cb), self, 0);
}

GtkWidget *
marker_page_sidebar_new (GListModel *documents)
{
  MarkerPageSidebar *self = g_object_new (MARKER_TYPE_PAGE_SIDEBAR, NULL);
  self->documents = g_object_ref (documents);
  g_signal_connect_object (documents, "items-changed", G_CALLBACK (documents_changed_cb), self, 0);
  documents_changed_cb (documents, 0, 0, g_list_model_get_n_items (documents), self);
  return GTK_WIDGET (self);
}
