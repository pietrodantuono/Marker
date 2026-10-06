/* Exercise real GTK workflows with an isolated profile, without adding test APIs
 * to the application. Reuse its startup wiring under a separate test entry point. */
#define main marker_cli_main
#include "../src/marker.c"
#undef main
#include "marker-page-sidebar.h"
#include "marker-workspace.h"
#include "marker-notebook-rail.h"
#include "marker-outline.h"
#include "marker-heading-gutter.h"
#include "marker-exporter.h"
#include "marker-markdown.h"
#include "scidown/src/charter/src/svg_utils.h"
#include <glib/gstdio.h>
#ifdef HAVE_XTEST
#include <gdk/x11/gdkx.h>
#include <X11/extensions/XTest.h>
#endif

static char *fixture;
static void capture_window (MarkerWindow *window, const char *name);

typedef struct { GMainLoop *loop; JSCValue *value; GError *error; } ScriptResult;

static void
script_finished (GObject *object, GAsyncResult *result, gpointer data)
{
  ScriptResult *wait = data;
  wait->value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (object), result, &wait->error);
  g_main_loop_quit (wait->loop);
}

static void
assert_script (MarkerPreview *preview, const char *script)
{
  g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
  ScriptResult wait = { .loop = loop };
  g_autoptr (GError) error = NULL;
  gboolean ready = marker_preview_wait_ready (preview, &error);
  g_assert_no_error (error);
  g_assert_true (ready);
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1, NULL, NULL,
                                      NULL, script_finished, &wait);
  g_main_loop_run (loop);
  g_assert_no_error (wait.error);
  g_assert_nonnull (wait.value);
  if (!jsc_value_to_boolean (wait.value))
    {
      g_test_message ("Failed preview expression: %s", script);
      g_clear_object (&wait.value);
      webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview),
        "JSON.stringify({color:getComputedStyle(document.body).color,"
        "math:!!document.querySelector('.katex'),diagram:!!document.querySelector('.mermaid svg'),"
        "fonts:['h2','.katex','pre code','p'].map(s=>{const e=document.querySelector(s);"
        "return e?getComputedStyle(e).fontFamily:null;})})", -1, NULL, NULL, NULL, script_finished, &wait);
      g_main_loop_run (loop);
      g_assert_no_error (wait.error);
      g_autofree char *details = jsc_value_to_string (wait.value);
      g_test_message ("Preview state: %s", details);
      g_object_unref (wait.value);
      g_assert_not_reached ();
    }
  g_assert_true (jsc_value_to_boolean (wait.value));
  g_object_unref (wait.value);
}

static void
settle (void)
{
  gint64 deadline = g_get_monotonic_time () + 350 * G_TIME_SPAN_MILLISECOND;
  do
    {
      while (g_main_context_iteration (NULL, FALSE));
      g_usleep (1000);
    }
  while (g_get_monotonic_time () < deadline);
}

static GtkWidget *
find_widget (GtkWidget *parent, GType type)
{
  if (g_type_is_a (G_OBJECT_TYPE (parent), type))
    return parent;
  for (GtkWidget *child = gtk_widget_get_first_child (parent); child != NULL; child = gtk_widget_get_next_sibling (child))
    {
      GtkWidget *found = find_widget (child, type);
      if (found != NULL)
        return found;
    }
  return NULL;
}

static AdwOverlaySplitView *
find_split (GtkWidget *parent, GType sidebar_type, const char *css)
{
  if (ADW_IS_OVERLAY_SPLIT_VIEW (parent))
    {
      GtkWidget *sidebar = adw_overlay_split_view_get_sidebar (ADW_OVERLAY_SPLIT_VIEW (parent));
      if ((sidebar_type != G_TYPE_INVALID && g_type_is_a (G_OBJECT_TYPE (sidebar), sidebar_type)) ||
          (css != NULL && gtk_widget_has_css_class (sidebar, css)))
        return ADW_OVERLAY_SPLIT_VIEW (parent);
    }
  for (GtkWidget *child = gtk_widget_get_first_child (parent); child != NULL; child = gtk_widget_get_next_sibling (child))
    {
      AdwOverlaySplitView *found = find_split (child, sidebar_type, css);
      if (found != NULL)
        return found;
    }
  return NULL;
}

static GtkWidget *
find_button (GtkWidget *parent, const char *label)
{
  if (GTK_IS_BUTTON (parent) && g_strcmp0 (gtk_button_get_label (GTK_BUTTON (parent)), label) == 0)
    return parent;
  for (GtkWidget *child = gtk_widget_get_first_child (parent); child != NULL; child = gtk_widget_get_next_sibling (child))
    {
      GtkWidget *found = find_button (child, label);
      if (found != NULL)
        return found;
    }
  return NULL;
}

static guint
count_widgets (GtkWidget *parent, GType type)
{
  guint count = g_type_is_a (G_OBJECT_TYPE (parent), type);
  for (GtkWidget *child = gtk_widget_get_first_child (parent); child != NULL; child = gtk_widget_get_next_sibling (child))
    count += count_widgets (child, type);
  return count;
}

static void
respond (AdwAlertDialog *dialog, const char *response)
{
  GtkWidget *button = find_button (GTK_WIDGET (dialog), adw_alert_dialog_get_response_label (dialog, response));
  g_assert_nonnull (button);
  g_signal_emit_by_name (button, "clicked");
}

static void
activate (MarkerWindow *window, const char *name)
{
  g_action_group_activate_action (G_ACTION_GROUP (window), name, NULL);
  settle ();
}

static void
notebook_action (MarkerWindow *window, const char *action)
{
  MarkerNotebookRail *rail = MARKER_NOTEBOOK_RAIL (find_widget (GTK_WIDGET (window), MARKER_TYPE_NOTEBOOK_RAIL));
  MarkerProject *project = marker_editor_get_project (marker_window_get_active_editor (window));
  g_signal_emit_by_name (rail, "notebook-action", action, project);
  settle ();
}

static MarkerWindow *
new_window (void)
{
  MarkerWindow *window = marker_window_new (app);
  gtk_window_present (GTK_WINDOW (window));
  settle ();
  return window;
}

static GFile *
fixture_file (const char *name)
{
  g_autofree char *path = g_build_filename (fixture, name, NULL);
  return g_file_new_for_path (path);
}

static void
test_navigation (void)
{
  MarkerWindow *window = marker_window_new (app);
  int width, height;
  gtk_window_get_default_size (GTK_WINDOW (window), &width, &height);
  g_assert_cmpint (width, ==, 1200);
  g_assert_cmpint (height, ==, 800);
  gtk_window_present (GTK_WINDOW (window));
  settle ();
  AdwOverlaySplitView *rail = find_split (GTK_WIDGET (window), MARKER_TYPE_NOTEBOOK_RAIL, NULL);
  AdwOverlaySplitView *pages = find_split (GTK_WIDGET (window), MARKER_TYPE_PAGE_SIDEBAR, NULL);
  AdwOverlaySplitView *outline = find_split (GTK_WIDGET (window), G_TYPE_INVALID, "marker-outline");
  g_assert_false (adw_overlay_split_view_get_collapsed (rail));
  g_assert_true (adw_overlay_split_view_get_show_sidebar (rail));
  g_assert_true (adw_overlay_split_view_get_show_sidebar (pages));
  g_assert_false (adw_overlay_split_view_get_show_sidebar (outline));
  activate (window, "outline");
  gtk_window_set_default_size (GTK_WINDOW (window), 900, 800);
  settle ();
  g_assert_false (adw_overlay_split_view_get_collapsed (rail));
  g_assert_true (adw_overlay_split_view_get_collapsed (pages));
  g_assert_true (adw_overlay_split_view_get_collapsed (outline));
  g_assert_false (adw_overlay_split_view_get_show_sidebar (pages));
  activate (window, "sidebar");
  g_assert_true (adw_overlay_split_view_get_show_sidebar (pages));
  activate (window, "outline");
  g_assert_false (adw_overlay_split_view_get_show_sidebar (pages));
  g_assert_true (adw_overlay_split_view_get_show_sidebar (outline));
  adw_overlay_split_view_set_show_sidebar (outline, FALSE); /* Overlay dismissal. */
  g_autoptr (MarkerSessionWindow) saved = marker_window_capture_session (window);
  g_assert_true (saved->pages_visible);
  g_assert_true (saved->outline_visible);
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 800);
  settle ();
  g_assert_true (adw_overlay_split_view_get_collapsed (rail));
  activate (window, "notebooks");
  g_assert_true (adw_overlay_split_view_get_show_sidebar (rail));
  activate (window, "sidebar");
  g_assert_false (adw_overlay_split_view_get_show_sidebar (rail));
  activate (window, "sidebar"); /* Explicitly hide pages. */
  gtk_window_set_default_size (GTK_WINDOW (window), 1200, 800);
  settle ();
  g_assert_false (adw_overlay_split_view_get_collapsed (pages));
  g_assert_false (adw_overlay_split_view_get_show_sidebar (pages));
  g_assert_true (adw_overlay_split_view_get_show_sidebar (outline));
  marker_window_restore_navigation (window, saved);
  g_assert_true (adw_overlay_split_view_get_show_sidebar (pages));
  activate (window, "focus");
  g_assert_false (adw_overlay_split_view_get_show_sidebar (rail));
  activate (window, "escape");
  g_assert_true (adw_overlay_split_view_get_show_sidebar (rail));
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}

static void
test_notebook_remove (void)
{
  MarkerWindow *window = new_window ();
  g_autoptr (GFile) root = fixture_file (NULL);
  g_autoptr (GFile) file = fixture_file ("notes.md");
  marker_window_add_project (window, root);
  marker_window_new_editor_from_file (window, file);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  gtk_text_buffer_insert_at_cursor (GTK_TEXT_BUFFER (marker_editor_get_buffer (editor)), "Edited ", -1);
  notebook_action (window, "remove");
  AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window)));
  g_assert_nonnull (dialog);
  respond (dialog, "cancel");
  settle ();
  g_autoptr (MarkerSessionWindow) saved = marker_window_capture_session (window);
  g_assert_cmpuint (saved->projects->len, ==, 1);
  g_assert_true (marker_window_get_active_editor (window) == editor);
  g_assert_true (marker_editor_has_unsaved_changes (editor));
  notebook_action (window, "remove");
  dialog = ADW_ALERT_DIALOG (adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window)));
  respond (dialog, "save");
  settle ();
  g_clear_pointer (&saved, marker_session_window_free);
  saved = marker_window_capture_session (window);
  g_assert_cmpuint (saved->projects->len, ==, 0);
  g_assert_cmpuint (saved->documents->len, ==, 0);
  g_assert_true (g_file_query_exists (root, NULL));
  g_assert_true (g_file_query_exists (file, NULL));
  marker_window_add_project (window, root);
  marker_window_new_editor (window);
  editor = marker_window_get_active_editor (window);
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (marker_editor_get_buffer (editor)), "Draft", -1);
  notebook_action (window, "remove");
  dialog = ADW_ALERT_DIALOG (adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window)));
  g_assert_nonnull (dialog);
  respond (dialog, "cancel");
  settle ();
  g_assert_true (marker_window_get_active_editor (window) == editor);
  notebook_action (window, "remove");
  dialog = ADW_ALERT_DIALOG (adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window)));
  respond (dialog, "remove");
  settle ();
  g_assert_null (marker_window_get_active_editor (window));
  g_assert_true (g_file_query_exists (root, NULL));
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}

static void
test_page_files_search (void)
{
  MarkerWindow *window = new_window ();
  g_autoptr (GFile) root = fixture_file (NULL);
  g_autoptr (GFile) file = fixture_file ("notes.md");
  marker_window_add_project (window, root);
  marker_window_new_editor_from_file (window, file);
  settle ();
  MarkerPageSidebar *sidebar = MARKER_PAGE_SIDEBAR (find_widget (GTK_WIDGET (window), MARKER_TYPE_PAGE_SIDEBAR));
  GtkListView *view = GTK_LIST_VIEW (find_widget (GTK_WIDGET (sidebar), GTK_TYPE_LIST_VIEW));
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (gtk_list_view_get_model (view))), ==, 2);
  marker_page_sidebar_set_files (sidebar, TRUE);
  settle ();
  g_assert_true (marker_page_sidebar_get_files (sidebar));
  GtkSearchEntry *search = GTK_SEARCH_ENTRY (find_widget (GTK_WIDGET (sidebar), GTK_TYPE_SEARCH_ENTRY));
  gtk_editable_set_text (GTK_EDITABLE (search), "nested.md");
  settle ();
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (gtk_list_view_get_model (view))), ==, 1);
  gtk_editable_set_text (GTK_EDITABLE (search), ".marker.css");
  settle ();
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (gtk_list_view_get_model (view))), ==, 1);
  marker_page_sidebar_set_files (sidebar, FALSE);
  settle ();
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (gtk_list_view_get_model (view))), ==, 0);
  gtk_editable_set_text (GTK_EDITABLE (search), "Renamed Page");
  marker_source_view_set_text (marker_editor_get_source_view (marker_window_get_active_editor (window)), "# Renamed Page\n", -1);
  settle ();
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (gtk_list_view_get_model (view))), ==, 1);
  marker_source_view_set_text (marker_editor_get_source_view (marker_window_get_active_editor (window)), "# Other Page\n", -1);
  settle ();
  g_assert_cmpuint (g_list_model_get_n_items (G_LIST_MODEL (gtk_list_view_get_model (view))), ==, 0);
  gtk_editable_set_text (GTK_EDITABLE (search), "");
  activate (window, "sidebar-refresh");
  GListModel *rows = G_LIST_MODEL (gtk_list_view_get_model (view));
  g_autoptr (GObject) first = g_list_model_get_item (rows, 0);
  g_assert_true (MARKER_IS_WORKSPACE_ITEM (first));
  g_assert_cmpstr (marker_workspace_item_get_name (MARKER_WORKSPACE_ITEM (first)), ==, "nested.md");
  GtkWidget *expander = find_widget (GTK_WIDGET (view), GTK_TYPE_TREE_EXPANDER);
  g_assert_nonnull (expander);
  g_autoptr (GListModel) controllers = gtk_widget_observe_controllers (expander);
  for (guint i = 0; i < g_list_model_get_n_items (controllers); i++)
    {
      g_autoptr (GtkEventController) controller = g_list_model_get_item (controllers, i);
      if (GTK_IS_GESTURE_CLICK (controller) &&
          gtk_gesture_single_get_button (GTK_GESTURE_SINGLE (controller)) == GDK_BUTTON_SECONDARY)
        g_signal_emit_by_name (controller, "pressed", 1, 10., 10.);
    }
  settle ();
  GtkWidget *open = find_button (expander, "Open");
  g_assert_nonnull (open);
  /* Opening an unopened row replaces its list model while the menu emits. */
  g_signal_emit_by_name (open, "clicked");
  settle ();
  g_autofree char *opened_name = g_file_get_basename (marker_editor_get_file (marker_window_get_active_editor (window)));
  g_assert_cmpstr (opened_name, ==, "nested.md");
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}

static void
test_default_typography (void)
{
  g_settings_reset (prefs.editor_settings, "prose-font");
  g_settings_reset (prefs.editor_settings, "writing-width");
  g_settings_reset (prefs.preview_settings, "css-toggle");
  g_settings_reset (prefs.preview_settings, "css-theme");
  MarkerWindow *window = new_window ();
  gtk_window_set_default_size (GTK_WINDOW (window), 1200, 800);
  marker_window_new_editor (window);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  MarkerSourceView *source = marker_editor_get_source_view (editor);
  MarkerPreview *preview = marker_editor_get_preview (editor);
  marker_source_view_set_text (source, "# A focused document\n\nDefault rendered text stays monospace.\n", -1);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  settle ();
  const PangoFontDescription *font = pango_context_get_font_description (
    gtk_widget_get_pango_context (GTK_WIDGET (source)));
  g_assert_cmpint (g_ascii_strcasecmp (pango_font_description_get_family (font), "monospace"), ==, 0);
  GtkSourceGutter *gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (source), GTK_TEXT_WINDOW_LEFT);
  int measure = gtk_widget_get_width (GTK_WIDGET (source)) - gtk_widget_get_width (GTK_WIDGET (gutter)) -
    gtk_text_view_get_left_margin (GTK_TEXT_VIEW (source)) - gtk_text_view_get_right_margin (GTK_TEXT_VIEW (source));
  g_assert_cmpint (measure, <=, 640);
  g_assert_cmpint (measure, >=, 620);
  marker_editor_set_view_mode (editor, PREVIEW_ONLY_MODE);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).fontFamily === 'monospace' && "
    "getComputedStyle(document.querySelector('p')).fontFamily === 'monospace' && "
    "getComputedStyle(document.body).maxWidth === '640px' && document.body.getBoundingClientRect().width <= 640");

  /* Stored preferences and selected themes still override fresh defaults. */
  g_settings_set_uint (prefs.editor_settings, "writing-width", 800);
  g_settings_set_string (prefs.editor_settings, "prose-font", "Serif 12");
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  marker_editor_apply_prefs (editor);
  settle ();
  font = pango_context_get_font_description (gtk_widget_get_pango_context (GTK_WIDGET (source)));
  g_assert_cmpint (g_ascii_strcasecmp (pango_font_description_get_family (font), "serif"), ==, 0);
  marker_editor_set_view_mode (editor, PREVIEW_ONLY_MODE);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).fontFamily === 'serif' && "
    "getComputedStyle(document.body).maxWidth === '800px'");
  marker_prefs_set_use_css_theme (TRUE);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).fontFamily.includes('Helvetica') && "
    "getComputedStyle(document.body).maxWidth === '800px'");
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  g_settings_reset (prefs.editor_settings, "prose-font");
  g_settings_reset (prefs.editor_settings, "writing-width");
  g_settings_reset (prefs.preview_settings, "css-toggle");
}

static void
test_document_surface (void)
{
  MarkerWindow *window = new_window ();
  marker_window_new_editor (window);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  MarkerSourceView *source = marker_editor_get_source_view (editor);
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (editor));
  const char *text = "# Héllo\n\n## Second\n\n```gnuplot\n# literal\n```\n\nSetext\n------\n";
  marker_source_view_set_text (source, text, -1);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  g_settings_set_boolean (prefs.editor_settings, "show-line-numbers", TRUE);
  marker_editor_apply_prefs (editor);
  activate (window, "outline");
  MarkerOutline *outline = MARKER_OUTLINE (find_widget (GTK_WIDGET (window), MARKER_TYPE_OUTLINE));
  GtkSourceGutter *gutter = gtk_source_view_get_gutter (GTK_SOURCE_VIEW (source), GTK_TEXT_WINDOW_LEFT);
  GtkWidget *heading_gutter = find_widget (GTK_WIDGET (gutter), MARKER_TYPE_HEADING_GUTTER);
  g_assert_nonnull (heading_gutter);
  g_assert_true (gtk_widget_get_visible (heading_gutter));
  g_assert_true (gtk_source_view_get_show_line_numbers (GTK_SOURCE_VIEW (source)));
  const MarkerMarkdownStructure *cached = marker_source_view_get_structure (source);
  g_assert_cmpuint (cached->headings->len, ==, 3);
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_line (buffer, &iter, 2);
  gtk_text_buffer_place_cursor (buffer, &iter);
  settle ();
  g_assert_true (cached == marker_source_view_get_structure (source));
  GtkDropDown *chooser = GTK_DROP_DOWN (find_widget (GTK_WIDGET (editor), GTK_TYPE_DROP_DOWN));
  g_assert_cmpuint (gtk_drop_down_get_selected (chooser), ==, 2);
  g_assert_cmpuint (count_widgets (GTK_WIDGET (outline), GTK_TYPE_BUTTON), ==, 3);
  gtk_drop_down_set_selected (chooser, 3);
  settle ();
  g_autofree char *changed = marker_source_view_get_text (source);
  g_assert_nonnull (strstr (changed, "### Second"));
  gtk_text_buffer_undo (buffer);
  settle ();
  g_autofree char *restored = marker_source_view_get_text (source);
  g_assert_cmpstr (restored, ==, text);
  gtk_text_buffer_get_start_iter (buffer, &iter);
  GdkRectangle cell = { 0, 50, 34, 35 };
  g_assert_true (gtk_source_gutter_renderer_query_activatable (GTK_SOURCE_GUTTER_RENDERER (heading_gutter), &iter, &cell));
  gtk_source_gutter_renderer_activate (GTK_SOURCE_GUTTER_RENDERER (heading_gutter), &iter, &cell, GDK_BUTTON_PRIMARY, 0, 1);
  settle ();
  GtkWidget *choice = find_button (heading_gutter, "Heading 4");
  g_assert_nonnull (choice);
  g_signal_emit_by_name (choice, "clicked");
  settle ();
  g_assert_cmpuint (((MarkerHeading *) g_ptr_array_index (marker_source_view_get_structure (source)->headings, 0))->level, ==, 4);
  gtk_text_buffer_undo (buffer);
  settle ();
  GtkTextIter start;
  gtk_text_buffer_get_start_iter (buffer, &start);
  gtk_text_buffer_insert (buffer, &start, "### Added\n\n", -1);
  settle ();
  g_assert_cmpuint (marker_source_view_get_structure (source)->headings->len, ==, 4);
  g_assert_cmpuint (count_widgets (GTK_WIDGET (outline), GTK_TYPE_BUTTON), ==, 4);
  g_assert_true (marker_editor_has_unsaved_changes (editor));
  for (MarkerViewMode mode = EDITOR_ONLY_MODE; mode <= FORMATTED_MODE; mode++)
    {
      marker_editor_set_view_mode (editor, mode);
      settle ();
      marker_editor_jump_to_heading (editor, 13);
      g_assert_cmpint (marker_source_view_get_cursor_position (source), ==, 13);
    }
  g_settings_set_boolean (prefs.editor_settings, "show-line-numbers", FALSE);
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}

static gboolean
has_spelling_error (GtkTextBuffer *buffer, int offset)
{
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
  GSList *tags = gtk_text_iter_get_tags (&iter);
  gboolean found = FALSE;
  for (GSList *item = tags; item != NULL; item = item->next)
    {
      int underline;
      g_object_get (item->data, "underline", &underline, NULL);
      found |= underline == PANGO_UNDERLINE_ERROR || underline == PANGO_UNDERLINE_ERROR_LINE;
    }
  g_slist_free (tags);
  return found;
}

static void
test_spell_check (void)
{
  if (!spelling_provider_supports_language (spelling_provider_get_default (), "en_US"))
    {
      g_test_skip ("An English dictionary is required to exercise spell checking");
      return;
    }

  MarkerWindow *window = new_window ();
  marker_window_new_editor (window);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  marker_editor_set_view_mode (editor, EDITOR_ONLY_MODE);
  MarkerSourceView *source = marker_editor_get_source_view (editor);
  GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (source));
  marker_source_view_set_spell_check_lang (source, "en_US");
  marker_source_view_set_spell_check (source, FALSE);
  marker_source_view_set_text (source, "A qzxqzx spelling example.\n", -1);
  GtkTextIter end;
  gtk_text_buffer_get_end_iter (buffer, &end);
  gtk_text_buffer_place_cursor (buffer, &end);
  settle ();
  marker_source_view_set_spell_check (source, TRUE);

  gint64 deadline = g_get_monotonic_time () + 5 * G_TIME_SPAN_SECOND;
  while (!has_spelling_error (buffer, 3) && g_get_monotonic_time () < deadline)
    settle ();
  g_assert_true (has_spelling_error (buffer, 3));
  g_assert_false (has_spelling_error (buffer, 12));
  marker_source_view_set_spell_check (source, FALSE);
  settle ();
  g_assert_false (has_spelling_error (buffer, 3));
  g_autofree char *text = marker_source_view_get_text (source);
  g_assert_cmpstr (text, ==, "A qzxqzx spelling example.\n");
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
}

static GtkWidget *
rich_region_recursive (GtkWidget *widget, guint *index)
{
  if (gtk_widget_has_css_class (widget, "marker-rich-region") && (*index)-- == 0)
    return widget;
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      GtkWidget *found = rich_region_recursive (child, index);
      if (found != NULL) return found;
    }
  return NULL;
}

static GtkWidget *
rich_region (MarkerSourceView *source, guint index)
{
  return rich_region_recursive (GTK_WIDGET (source), &index);
}

static void
wait_rich_pictures (MarkerSourceView *source, guint count)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  gboolean ready;
  do
    {
      while (g_main_context_iteration (NULL, FALSE));
      ready = TRUE;
      for (guint i = 0; i < count; i++)
        {
          GtkWidget *region = rich_region (source, i);
          GtkWidget *picture = region != NULL ? find_widget (region, GTK_TYPE_PICTURE) : NULL;
          ready &= picture != NULL && gtk_picture_get_paintable (GTK_PICTURE (picture)) != NULL;
        }
      if (ready) break;
      g_usleep (1000);
    } while (g_get_monotonic_time () < deadline);
  if (!ready)
    for (guint i = 0; i < count; i++)
      {
        GtkWidget *region = rich_region (source, i);
        g_test_message ("Rich region %u: %s", i, region != NULL ? "present" : "missing");
        if (region != NULL)
          {
            for (GtkWidget *child = gtk_widget_get_first_child (region); child != NULL;
                 child = gtk_widget_get_next_sibling (child))
              if (GTK_IS_LABEL (child)) g_test_message ("Region message: %s", gtk_label_get_text (GTK_LABEL (child)));
          }
      }
  g_assert_true (ready);
}

#ifdef HAVE_XTEST
static void
pointer_button (MarkerWindow *window, GtkWidget *target, gboolean pressed)
{
  GdkSurface *surface = gtk_native_get_surface (GTK_NATIVE (window));
  Display *display = gdk_x11_display_get_xdisplay (gdk_surface_get_display (surface));
  if (pressed)
    {
      graphene_rect_t bounds;
      g_assert_nonnull (target);
      g_assert_true (gtk_widget_compute_bounds (target, GTK_WIDGET (window), &bounds));
      int x, y;
      Window child;
      XTranslateCoordinates (display, gdk_x11_surface_get_xid (surface),
                             DefaultRootWindow (display), 0, 0, &x, &y, &child);
      double local_x = bounds.origin.x + bounds.size.width / 2;
      double local_y = bounds.origin.y + bounds.size.height / 2;
      g_assert_cmpfloat (local_y, >=, 0);
      g_assert_cmpfloat (local_y, <, gtk_widget_get_height (GTK_WIDGET (window)));
      GtkWidget *picked = gtk_widget_pick (GTK_WIDGET (window), local_x, local_y, GTK_PICK_DEFAULT);
      g_assert_true (picked == target || gtk_widget_is_ancestor (picked, target));
      XTestFakeMotionEvent (display, -1, x + local_x, y + local_y, CurrentTime);
    }
  XTestFakeButtonEvent (display, 1, pressed, CurrentTime);
  XFlush (display);
  settle ();
}
#endif

static void
test_rich_pointer (void)
{
#ifdef HAVE_XTEST
  GdkDisplay *display = gdk_display_get_default ();
  if (!GDK_IS_X11_DISPLAY (display))
    { g_test_skip ("Pointer injection requires an X11 test display"); return; }
  Display *xdisplay = gdk_x11_display_get_xdisplay (display);
  int event, error, major, minor;
  if (!XTestQueryExtension (xdisplay, &event, &error, &major, &minor))
    { g_test_skip ("XTest is unavailable on the test display"); return; }
  g_autofree char *path = g_build_filename (fixture, "pointer.md", NULL);
  marker_prefs_set_use_dark_theme (FALSE);
  g_settings_set_boolean (prefs.preview_settings, "css-toggle", FALSE);
  const char *markdown = "# Pointer test\n\n![Image](data/paper.svg)\n\nEnd.\n";
  g_assert_true (g_file_set_contents (path, markdown, -1, NULL));
  g_autoptr (GFile) file = g_file_new_for_path (path);
  MarkerWindow *window = new_window ();
  marker_window_new_editor_from_file (window, file);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  MarkerSourceView *source = marker_editor_get_source_view (editor);
  wait_rich_pictures (source, 1);
  settle ();
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (editor));
  for (guint i = 0; i < 4; i++)
    {
      const char *before = i % 2 == 0 ? "Source" : "Render";
      const char *after = i % 2 == 0 ? "Render" : "Source";
      int cursor = marker_source_view_get_cursor_position (source);
      pointer_button (window, find_button (GTK_WIDGET (editor), before), TRUE);
      /* Press must not let the ancestor text view move the caret or toggle
       * presentation before the native button's release activation. */
      g_assert_nonnull (find_button (GTK_WIDGET (editor), before));
      g_assert_cmpint (marker_source_view_get_cursor_position (source), ==, cursor);
      pointer_button (window, NULL, FALSE);
      g_assert_nonnull (find_button (GTK_WIDGET (editor), after));
      if (i % 2) wait_rich_pictures (source, 1);
      else g_assert_null (rich_region (source, 0));
      g_assert_false (gtk_text_buffer_get_has_selection (buffer));
      marker_editor_refresh_preview (editor);
      if (i % 2) wait_rich_pictures (source, 1);
      settle ();
      g_assert_nonnull (find_button (GTK_WIDGET (editor), after));
    }
  /* Dragging away cancels a button click rather than starting a text selection. */
  pointer_button (window, find_button (GTK_WIDGET (editor), "Source"), TRUE);
  XTestFakeMotionEvent (xdisplay, -1, 1500, 950, CurrentTime);
  pointer_button (window, NULL, FALSE);
  g_assert_nonnull (find_button (GTK_WIDGET (editor), "Source"));
  g_assert_false (gtk_text_buffer_get_has_selection (buffer));
  /* A rendered image must also own its click sequence. */
  pointer_button (window, find_widget (rich_region (source, 0), GTK_TYPE_PICTURE), TRUE);
  g_assert_nonnull (find_button (GTK_WIDGET (editor), "Source"));
  pointer_button (window, NULL, FALSE);
  g_assert_nonnull (find_button (GTK_WIDGET (editor), "Render"));
  g_assert_false (gtk_text_buffer_get_has_selection (buffer));
  g_assert_null (rich_region (source, 0));
  g_autofree char *unchanged = marker_source_view_get_text (source);
  g_assert_cmpstr (unchanged, ==, markdown);
  g_assert_false (marker_editor_has_unsaved_changes (editor));
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  g_remove (path);
#else
  g_test_skip ("Build with optional GTK X11/XTest development libraries for pointer regression");
#endif
}

static void
render_count_cb (MarkerPreview *preview, guint *count)
{
  (*count)++;
}

static gboolean
click_button_cb (gpointer button)
{
  g_signal_emit_by_name (button, "clicked");
  return G_SOURCE_REMOVE;
}

static void
test_rich_editing (void)
{
  g_settings_set_boolean (prefs.preview_settings, "mathjs-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "mermaid-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "gnuplot-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "charter-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "css-toggle", FALSE);
  marker_prefs_set_math_backend (KATEX);
  marker_prefs_set_use_dark_theme (FALSE);
  g_autofree char *path = g_build_filename (fixture, "rich.md", NULL);
  g_autofree char *image = g_build_filename (fixture, "data", "inline-image.svg", NULL);
  g_file_set_contents (image,
    "<svg xmlns='http://www.w3.org/2000/svg' width='300' height='100'><rect width='300' height='100' rx='8' fill='#d8e9fa'/><path d='M20 80L90 30L160 55L240 15L280 35' fill='none' stroke='#1c71d8' stroke-width='4'/></svg>", -1, NULL);
  const char *markdown = "# Rich elements\n\n![Héllo](data/inline-image.svg)\n\n"
    "Inline $x^2$ stays with its paragraph.\n\n"
    "| Name | Value |\n| --- | ---: |\n| A | 12 |\n\n"
    "```gnuplot\nset samples 20\nplot sin(x)\n```\n\n"
    "```mermaid\ngraph TD\n A --> B\n```\n\n"
    "```charter\nplot\n x range: 1 10 3\n y math: 2*x\n```\n\n"
    "$$\nx^2 + y^2\n$$\n\n```math\n\\frac{1}{2}\n```\n\nEnd.\n";
  g_assert_true (g_file_set_contents (path, markdown, -1, NULL));
  g_autoptr (GFile) file = g_file_new_for_path (path);
  MarkerWindow *window = new_window ();
  marker_window_new_editor_from_file (window, file);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  MarkerSourceView *source = marker_editor_get_source_view (editor);
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (editor));
  wait_rich_pictures (source, 8);
  settle ();
  MarkerPreview *renderer = MARKER_PREVIEW (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  assert_script (renderer, "document.querySelectorAll('.katex').length >= 3 && "
    "!!document.querySelector('.gnuplot-preview svg') && !!document.querySelector('.mermaid svg') && "
    "!!document.querySelector('svg.charter') && document.querySelector('img').naturalWidth > 0");
  for (guint i = 0; i < 8; i++)
    {
      GtkWidget *region = rich_region (source, i);
      g_assert_null (find_button (region, "Source"));
      g_assert_true (gtk_widget_get_visible (find_widget (region, GTK_TYPE_PICTURE)));
    }
  capture_window (window, "rich-light");
  guint renders = 0;
  MarkerPreview *full = marker_editor_get_preview (editor);
  /* Source also cancels a full-preview readiness wait (e.g. Print preparation). */
  marker_preview_render_markdown (full, markdown, NULL, path, NULL, -1);
  g_idle_add (click_button_cb, find_button (GTK_WIDGET (editor), "Source"));
  g_autoptr (GError) paused = NULL;
  g_assert_false (marker_preview_wait_ready (full, &paused));
  g_assert_error (paused, G_IO_ERROR, G_IO_ERROR_CLOSED);
  gulong handler = g_signal_connect (full, "render-complete", G_CALLBACK (render_count_cb), &renders);
  settle ();
  g_assert_null (rich_region (source, 0));
  g_assert_null (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  g_assert_false (webkit_settings_get_enable_javascript (webkit_web_view_get_settings (WEBKIT_WEB_VIEW (full))));
  gint64 blank_deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (g_strcmp0 (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (full)), "about:blank") != 0 &&
         g_get_monotonic_time () < blank_deadline)
    {
      while (g_main_context_iteration (NULL, FALSE));
      g_usleep (1000);
    }
  g_assert_cmpstr (webkit_web_view_get_uri (WEBKIT_WEB_VIEW (full)), ==, "about:blank");
  g_assert_false (gtk_text_buffer_get_has_selection (buffer));
  GtkTextIter iter;
  gtk_text_buffer_get_start_iter (buffer, &iter);
  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_insert (buffer, &iter, "Introduction\n\n", -1);
  gtk_text_buffer_end_user_action (buffer);
  settle ();
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 800);
  marker_prefs_set_use_dark_theme (TRUE);
  marker_editor_apply_prefs (editor);
  settle ();
  gtk_text_buffer_undo (buffer);
  marker_editor_refresh_preview (editor);
  settle ();
  g_assert_cmpuint (renders, ==, 0);
  g_assert_null (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  g_assert_nonnull (find_button (GTK_WIDGET (editor), "Render"));
  g_autofree char *restored = marker_source_view_get_text (source);
  g_assert_cmpstr (restored, ==, markdown);
  g_assert_false (marker_editor_has_unsaved_changes (editor));
  AdwWindowTitle *title = ADW_WINDOW_TITLE (find_widget (GTK_WIDGET (window), ADW_TYPE_WINDOW_TITLE));
  g_assert_cmpstr (adw_window_title_get_title (title), ==, "Rich elements");
  /* Source stays selected across view-mode transitions, and resume renders once. */
  marker_editor_set_view_mode (editor, EDITOR_ONLY_MODE);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  settle ();
  g_assert_null (rich_region (source, 0));
  g_signal_handler_disconnect (full, handler);
  gtk_window_set_default_size (GTK_WINDOW (window), 1200, 800);
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Render"), "clicked");
  wait_rich_pictures (source, 8);
  settle ();
  capture_window (window, "rich-dark");
  /* Both math backends share inline/display/fenced regions and readiness. */
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Source"), "clicked");
  marker_prefs_set_math_backend (MATHJAX);
  marker_editor_apply_prefs (editor);
  g_assert_null (rich_region (source, 0));
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Render"), "clicked");
  wait_rich_pictures (source, 8);
  renderer = MARKER_PREVIEW (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  assert_script (renderer, "document.querySelectorAll('mjx-container svg,.MathJax').length >= 3 && "
    "!!document.querySelector('svg.charter') && !!document.querySelector('.mermaid svg')");
  gtk_window_set_default_size (GTK_WINDOW (window), 500, 800);
  settle ();
  wait_rich_pictures (source, 8);
  capture_window (window, "rich-narrow");
  marker_prefs_set_math_backend (KATEX);
  marker_prefs_set_use_dark_theme (FALSE);
  g_assert_true (marker_editor_save_file (editor));
  g_autofree char *saved = NULL;
  g_assert_true (g_file_get_contents (path, &saved, NULL, NULL));
  g_assert_cmpstr (saved, ==, markdown);
  /* Individual scientific preferences remain effective in page Render. */
  const char *renderers[] = { "mathjs-toggle", "mermaid-toggle", "gnuplot-toggle", "charter-toggle" };
  for (guint i = 0; i < G_N_ELEMENTS (renderers); i++)
    g_settings_set_boolean (prefs.preview_settings, renderers[i], FALSE);
  marker_editor_apply_prefs (editor);
  renderer = MARKER_PREVIEW (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  assert_script (renderer, "!document.querySelector('.katex,mjx-container,.MathJax,svg.charter,.mermaid svg,.gnuplot-preview svg')");
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time () < deadline)
    {
      while (g_main_context_iteration (NULL, FALSE));
      GtkWidget *label = find_widget (rich_region (source, 5), GTK_TYPE_LABEL);
      if (g_str_equal (gtk_label_get_text (GTK_LABEL (label)),
                       "Renderer disabled or unavailable. Check Preview preferences.")) break;
      g_usleep (1000);
    }
  for (guint i = 3; i <= 5; i++)
    {
      GtkWidget *region = rich_region (source, i);
      g_assert_cmpstr (gtk_label_get_text (GTK_LABEL (find_widget (region, GTK_TYPE_LABEL))), ==,
                      "Renderer disabled or unavailable. Check Preview preferences.");
      g_assert_false (gtk_widget_get_visible (find_widget (region, GTK_TYPE_PICTURE)));
    }
  for (guint i = 0; i < G_N_ELEMENTS (renderers); i++)
    g_settings_set_boolean (prefs.preview_settings, renderers[i], TRUE);
  /* Cancel a newly started render through Source, then close without stale callbacks. */
  marker_editor_refresh_preview (editor);
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Source"), "clicked");
  settle ();
  g_assert_null (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  g_remove (path);
  g_remove (image);
}

static void
wait_rich_refresh (MarkerSourceView *source, guint index, GdkPaintable *previous)
{
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (g_get_monotonic_time () < deadline)
    {
      while (g_main_context_iteration (NULL, FALSE));
      GtkWidget *region = rich_region (source, index);
      GdkPaintable *current = region != NULL ? gtk_picture_get_paintable (
        GTK_PICTURE (find_widget (region, GTK_TYPE_PICTURE))) : NULL;
      if (current != NULL && current != previous) return;
      g_usleep (1000);
    }
  g_assert_not_reached ();
}

static void
test_rich_failures_refresh (void)
{
  g_settings_set_boolean (prefs.preview_settings, "mathjs-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "gnuplot-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "css-toggle", FALSE);
  g_autofree char *path = g_build_filename (fixture, "rich-refresh.md", NULL);
  g_autofree char *image = g_build_filename (fixture, "missing.svg", NULL);
  g_autofree char *style = g_build_filename (fixture, ".marker.css", NULL);
  g_autofree char *data = g_build_filename (fixture, "inline.csv", NULL);
  const char *markdown = "# Refresh\n\n![Asset](missing.svg)\n\n"
    "```gnuplot\nthis is invalid\n```\n\n"
    "```math\nx^2 + y^2\n```\n\n"
    "```gnuplot\nset datafile separator ','\nplot 'inline.csv' using 1:2 with lines\n```\n\nEnd.\n";
  g_file_set_contents (path, markdown, -1, NULL);
  g_file_set_contents (data, "0,1\n1,2\n2,3\n", -1, NULL);
  MarkerWindow *window = new_window ();
  g_autoptr (GFile) root = fixture_file (NULL);
  marker_window_add_project (window, root);
  g_autoptr (GFile) file = g_file_new_for_path (path);
  marker_window_new_editor_from_file (window, file);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  MarkerSourceView *source = marker_editor_get_source_view (editor);
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  do
    {
      settle ();
      if (rich_region (source, 3) != NULL &&
          !gtk_widget_get_visible (find_widget (rich_region (source, 0), GTK_TYPE_PICTURE)) &&
          !gtk_widget_get_visible (find_widget (rich_region (source, 1), GTK_TYPE_PICTURE)) &&
          gtk_picture_get_paintable (GTK_PICTURE (find_widget (rich_region (source, 2), GTK_TYPE_PICTURE))) != NULL &&
          gtk_picture_get_paintable (GTK_PICTURE (find_widget (rich_region (source, 3), GTK_TYPE_PICTURE))) != NULL) break;
    } while (g_get_monotonic_time () < deadline);
  for (guint i = 0; i < 2; i++)
    g_assert_false (gtk_widget_get_visible (find_widget (rich_region (source, i), GTK_TYPE_PICTURE)));
  for (guint i = 2; i < 4; i++)
    g_assert_nonnull (gtk_picture_get_paintable (GTK_PICTURE (find_widget (rich_region (source, i), GTK_TYPE_PICTURE))));
  g_assert_false (marker_editor_has_unsaved_changes (editor));
  /* Recover each failed region without losing source or changing other blocks. */
  g_file_set_contents (image, "<svg xmlns='http://www.w3.org/2000/svg' width='120' height='60'><rect width='120' height='60' fill='#3584e4'/></svg>", -1, NULL);
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Source"), "clicked");
  settle ();
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (editor));
  GtkTextIter start, end;
  guint offset = g_utf8_pointer_to_offset (markdown, strstr (markdown, "this is invalid"));
  gtk_text_buffer_get_iter_at_offset (buffer, &start, offset);
  gtk_text_buffer_get_iter_at_offset (buffer, &end, offset + strlen ("this is invalid"));
  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_delete (buffer, &start, &end);
  gtk_text_buffer_insert (buffer, &start, "plot cos(x)", -1);
  gtk_text_buffer_end_user_action (buffer);
  settle ();
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Render"), "clicked");
  wait_rich_pictures (source, 4);
  MarkerPreview *renderer = MARKER_PREVIEW (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  assert_script (renderer, "document.querySelector('.marker-math-block .katex') !== null && "
    "document.querySelectorAll('.gnuplot-preview svg').length === 2 && !document.querySelector('.gnuplot-error')");
  g_autofree char *edited = marker_source_view_get_text (source);
  g_assert_nonnull (strstr (edited, "plot cos(x)"));
  g_autoptr (GdkPaintable) old_style = g_object_ref (gtk_picture_get_paintable (
    GTK_PICTURE (find_widget (rich_region (source, 3), GTK_TYPE_PICTURE))));
  /* Notebook stylesheet atomic changes use the same renderer, without typing. */
  g_file_set_contents (style, "body { color: rgb(12, 34, 56); }", -1, NULL);
  wait_rich_refresh (source, 3, old_style);
  wait_rich_pictures (source, 4);
  assert_script (renderer, "getComputedStyle(document.body).color === 'rgb(12, 34, 56)'");
  g_autoptr (GdkPaintable) old_plot = g_object_ref (gtk_picture_get_paintable (
    GTK_PICTURE (find_widget (rich_region (source, 3), GTK_TYPE_PICTURE))));
  g_file_set_contents (data, "0,3\n1,2\n2,1\n", -1, NULL);
  wait_rich_refresh (source, 3, old_plot);
  wait_rich_pictures (source, 4);
  g_assert_true (old_plot != gtk_picture_get_paintable (GTK_PICTURE (find_widget (rich_region (source, 3), GTK_TYPE_PICTURE))));
  g_autofree char *after_refresh = marker_source_view_get_text (source);
  g_assert_cmpstr (after_refresh, ==, edited);
  g_assert_true (marker_editor_save_file (editor));
  /* Export renders explicitly without resuming a paused writing page. */
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Source"), "clicked");
  settle ();
  g_autofree char *html_path = g_build_filename (fixture, "rich-inline.html", NULL);
  g_autofree char *pdf_path = g_build_filename (fixture, "rich-inline.pdf", NULL);
  g_assert_true (marker_exporter_export (path, html_path));
  g_autofree char *html = NULL;
  g_file_get_contents (html_path, &html, NULL, NULL);
  g_assert_nonnull (strstr (html, "gnuplot-preview"));
  g_assert_nonnull (strstr (html, "marker-math-block"));
  g_assert_null (strstr (html, "markerRichSnapshot"));
  g_assert_null (strstr (html, "marker-rich-region"));
  g_assert_true (marker_exporter_export (path, pdf_path));
  g_autofree char *pdf = NULL;
  g_file_get_contents (pdf_path, &pdf, NULL, NULL);
  g_assert_true (g_str_has_prefix (pdf, "%PDF-"));
  g_assert_null (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  g_assert_nonnull (find_button (GTK_WIDGET (editor), "Render"));
  g_file_set_contents (data, "0,5\n1,4\n2,6\n", -1, NULL);
  g_file_set_contents (style, "body { color: rgb(34, 56, 78); }", -1, NULL);
  settle ();
  g_assert_null (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  g_assert_nonnull (find_button (GTK_WIDGET (editor), "Render"));
  g_signal_emit_by_name (find_button (GTK_WIDGET (editor), "Render"), "clicked");
  wait_rich_pictures (source, 4);
  renderer = MARKER_PREVIEW (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  assert_script (renderer, "getComputedStyle(document.body).color === 'rgb(34, 56, 78)' && "
    "document.querySelectorAll('.gnuplot-preview svg').length === 2");
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  /* Unsaved notebook pages resolve assets against their notebook root. */
  window = new_window ();
  marker_window_add_project (window, root);
  marker_window_new_editor (window);
  editor = marker_window_get_active_editor (window);
  source = marker_editor_get_source_view (editor);
  marker_source_view_set_text (source, "![Draft asset](missing.svg)\n", -1);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  wait_rich_pictures (source, 1);
  renderer = MARKER_PREVIEW (find_widget (GTK_WIDGET (source), MARKER_TYPE_PREVIEW));
  assert_script (renderer, "document.querySelector('img').naturalWidth === 120");
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  g_file_set_contents (style, "body { color: #123456; }", -1, NULL);
  g_remove (path); g_remove (image); g_remove (data);
  g_remove (html_path); g_remove (pdf_path);
}

static void
test_notebook_styles (void)
{
  g_settings_set_boolean (prefs.preview_settings, "mathjs-toggle", FALSE);
  g_settings_set_boolean (prefs.preview_settings, "mermaid-toggle", FALSE);
  g_settings_set_boolean (prefs.preview_settings, "highlight-toggle", FALSE);
  g_settings_set_boolean (prefs.preview_settings, "gnuplot-toggle", FALSE);
  g_settings_set_boolean (prefs.window_settings, "follow-system-theme", FALSE);
  marker_prefs_set_use_dark_theme (TRUE);
  g_settings_set_boolean (prefs.preview_settings, "override-text-font", TRUE);
  g_settings_set_string (prefs.preview_settings, "text-font", "serif 18");
  g_autofree char *global = g_build_filename (fixture, "global.css", NULL);
  g_file_set_contents (global, "body { color: #884422; font-family: serif; }", -1, NULL);
  marker_prefs_set_css_theme (global);
  marker_prefs_set_use_css_theme (TRUE);
  g_autofree char *style = g_build_filename (fixture, ".marker.css", NULL);
  g_remove (style);
  MarkerWindow *window = new_window ();
  g_autoptr (GFile) root = fixture_file (NULL);
  g_autoptr (GFile) file = fixture_file ("data/nested.md");
  marker_window_add_project (window, root);
  marker_window_new_editor_from_file (window, file);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  MarkerProject *project = marker_editor_get_project (editor);
  /* A second window must reuse the live model without restoring stale appearance. */
  marker_project_set_name (project, "Live Notebook");
  MarkerWindow *other = new_window ();
  marker_window_add_project (other, root);
  marker_window_new_editor (other);
  g_assert_true (marker_editor_get_project (marker_window_get_active_editor (other)) == project);
  g_assert_cmpstr (marker_project_get_name (project), ==, "Live Notebook");
  gtk_window_destroy (GTK_WINDOW (other));
  MarkerNotebookRail *rail = MARKER_NOTEBOOK_RAIL (find_widget (GTK_WIDGET (window), MARKER_TYPE_NOTEBOOK_RAIL));
  g_signal_emit_by_name (rail, "notebook-action", "styles", project);
  settle ();
  MarkerEditor *css_editor = marker_window_get_active_editor (window);
  g_assert_true (g_file_test (style, G_FILE_TEST_IS_REGULAR));
  g_assert_cmpint (marker_editor_get_view_mode (css_editor), ==, EDITOR_ONLY_MODE);
  marker_editor_set_view_mode (css_editor, FORMATTED_MODE);
  g_assert_cmpint (marker_editor_get_view_mode (css_editor), ==, EDITOR_ONLY_MODE);
  gtk_text_buffer_set_text (GTK_TEXT_BUFFER (marker_editor_get_buffer (css_editor)),
    "body { color: #123456; font-family: monospace; max-width: 680px; background-image: url('data/paper.svg'); }", -1);
  g_assert_true (marker_editor_save_file (css_editor));
  g_assert_false (g_action_group_get_action_enabled (G_ACTION_GROUP (window), "view-mode"));
  g_autoptr (GFile) renamed = fixture_file ("styles.md");
  g_assert_true (marker_editor_save_file_as (css_editor, renamed));
  g_assert_cmpstr (gtk_source_language_get_id (gtk_source_buffer_get_language (marker_editor_get_buffer (css_editor))), ==, "markdown");
  marker_editor_set_view_mode (css_editor, FORMATTED_MODE);
  g_autoptr (GFile) stylesheet = fixture_file (".marker.css");
  g_assert_true (marker_editor_save_file_as (css_editor, stylesheet));
  g_assert_cmpint (marker_editor_get_view_mode (css_editor), ==, EDITOR_ONLY_MODE);
  g_assert_cmpstr (gtk_source_language_get_id (gtk_source_buffer_get_language (marker_editor_get_buffer (css_editor))), ==, "css");
  g_file_delete (renamed, NULL, NULL);
  settle ();
  marker_editor_set_view_mode (editor, PREVIEW_ONLY_MODE);
  MarkerPreview *preview = marker_editor_get_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(18, 52, 86)' && "
    "getComputedStyle(document.body).fontFamily === 'monospace' && "
    "getComputedStyle(document.body).maxWidth === '680px' && "
    "getComputedStyle(document.body).backgroundImage.includes('/data/paper.svg') && "
    "document.querySelectorAll('#cursor_pos').length === 1");
  guint renders = 0;
  gulong handler = g_signal_connect (preview, "render-complete", G_CALLBACK (render_count_cb), &renders);
  g_file_set_contents (style, "body { color: #654321; }", -1, NULL); /* Atomic replacement. */
  gint64 deadline = g_get_monotonic_time () + 20 * G_TIME_SPAN_SECOND;
  while (renders == 0 && g_get_monotonic_time () < deadline)
    {
      while (g_main_context_iteration (NULL, FALSE));
      g_usleep (1000);
    }
  g_assert_cmpuint (renders, >, 0);
  g_signal_handler_disconnect (preview, handler);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(101, 67, 33)'");
  g_autofree char *outfile = g_build_filename (fixture, "export.html", NULL);
  g_autofree char *infile = g_file_get_path (file);
  g_assert_true (marker_exporter_export (infile, outfile));
  g_autofree char *html = NULL;
  g_assert_true (g_file_get_contents (outfile, &html, NULL, NULL));
  g_assert_nonnull (strstr (html, "marker-notebook"));
  g_assert_nonnull (strstr (html, ".marker.css?marker="));
  g_assert_nonnull (strstr (html, "<base href="));
  g_autoptr (GFile) exported_file = fixture_file ("export.html");
  g_autofree char *export_uri = g_file_get_uri (exported_file);
  webkit_web_view_load_uri (WEBKIT_WEB_VIEW (preview), export_uri);
  settle ();
  /* Use the real standalone exported page, not another preview render. */
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(101, 67, 33)'");
  g_remove (style);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(222, 221, 218)'");
  g_file_set_contents (style, "body { color: #123456; }", -1, NULL);
  settle ();
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(18, 52, 86)'");
  g_file_set_contents (global, "body { color: #cc0000 !important; }", -1, NULL);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(204, 0, 0)'");
  g_file_set_contents (global, "body { color: #884422; }", -1, NULL);
  marker_editor_refresh_preview (editor);
  g_autofree char *pdf = g_build_filename (fixture, "export.pdf", NULL);
  g_assert_true (marker_exporter_export (infile, pdf));
  g_autofree char *pdf_contents = NULL;
  gsize size;
  g_assert_true (g_file_get_contents (pdf, &pdf_contents, &size, NULL));
  g_assert_cmpuint (size, >, 100);
  g_assert_true (g_str_has_prefix (pdf_contents, "%PDF-"));
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  marker_prefs_set_use_dark_theme (FALSE);
  g_settings_reset (prefs.preview_settings, "override-text-font");
  g_settings_reset (prefs.preview_settings, "css-theme");
}

static void
test_preview_preferences (void)
{
  g_autofree char *style = g_build_filename (fixture, ".marker.css", NULL);
  g_autofree char *document_css = g_build_filename (fixture, "data", "document.css", NULL);
  g_remove (style);
  g_file_set_contents (document_css, "body { color: #006699; }", -1, NULL);
  marker_prefs_set_use_css_theme (FALSE);
  g_settings_set_boolean (prefs.preview_settings, "mathjs-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "highlight-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "mermaid-toggle", TRUE);
  g_settings_set_boolean (prefs.preview_settings, "gnuplot-toggle", FALSE);
  const char *roles[] = { "header", "math", "code", "text" };
  const char *fonts[] = { "serif 20", "serif 15", "monospace 14", "sans-serif 16" };
  for (guint i = 0; i < G_N_ELEMENTS (roles); i++)
    {
      g_autofree char *toggle = g_strdup_printf ("override-%s-font", roles[i]);
      g_autofree char *key = g_strdup_printf ("%s-font", roles[i]);
      g_settings_set_boolean (prefs.preview_settings, toggle, TRUE);
      g_settings_set_string (prefs.preview_settings, key, fonts[i]);
    }
  MarkerWindow *window = new_window ();
  g_autoptr (GFile) root = fixture_file (NULL);
  g_autoptr (GFile) file = fixture_file ("data/nested.md");
  marker_window_add_project (window, root);
  marker_window_new_editor_from_file (window, file);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  MarkerPreview *preview = marker_editor_get_preview (editor);
  marker_editor_set_view_mode (editor, PREVIEW_ONLY_MODE);
  marker_source_view_set_text (marker_editor_get_source_view (editor),
    "---\nstyle: document.css\n---\n# Heading\n\nText with $x^2$.\n\n"
    "```python\nx = 2\n```\n\n```mermaid\ngraph TD; A-->B;\n```\n", -1);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "document.querySelector('.katex') !== null && document.querySelector('.mermaid svg') !== null && "
    "getComputedStyle(document.querySelector('h2')).fontFamily === 'serif' && "
    "getComputedStyle(document.querySelector('.katex')).fontFamily === 'serif' && "
    "getComputedStyle(document.querySelector('pre code')).fontFamily === 'monospace' && "
    "getComputedStyle(document.querySelector('p')).fontFamily === 'sans-serif' && "
    "getComputedStyle(document.body).color === 'rgb(0, 102, 153)'");
  g_file_set_contents (style, "body { color: #123456; }", -1, NULL);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(18, 52, 86)'");
  g_remove (style);
  marker_source_view_set_text (marker_editor_get_source_view (editor), "# Theme\n\nA paragraph.\n", -1);
  marker_prefs_set_use_dark_theme (TRUE);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(222, 221, 218)'");
  marker_prefs_set_use_dark_theme (FALSE);
  marker_editor_refresh_preview (editor);
  assert_script (preview, "getComputedStyle(document.body).color === 'rgb(48, 48, 54)'");
  g_settings_set_boolean (prefs.window_settings, "follow-system-theme", TRUE);
  g_assert_cmpint (adw_style_manager_get_color_scheme (adw_style_manager_get_default ()), ==, ADW_COLOR_SCHEME_DEFAULT);
  g_assert_cmpint (marker_prefs_get_use_dark_theme (), ==, adw_style_manager_get_dark (adw_style_manager_get_default ()));
  g_settings_set_boolean (prefs.window_settings, "follow-system-theme", FALSE);
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  for (guint i = 0; i < G_N_ELEMENTS (roles); i++)
    {
      g_autofree char *toggle = g_strdup_printf ("override-%s-font", roles[i]);
      g_autofree char *key = g_strdup_printf ("%s-font", roles[i]);
      g_settings_reset (prefs.preview_settings, toggle);
      g_settings_reset (prefs.preview_settings, key);
    }
  g_remove (document_css);
}

static void
test_include_files (void)
{
  /* Neither the notebook root nor the working directory fits the old fixed
   * include-path buffers. Missing, empty and non-file targets must be harmless. */
  g_autofree char *component = g_strnfill (200, 'a');
  g_autofree char *directory = g_build_filename (fixture, component, component, NULL);
  g_autofree char *included = g_build_filename (directory, "included.md", NULL);
  g_autofree char *empty = g_build_filename (directory, "empty.md", NULL);
  g_assert_cmpint (g_mkdir_with_parents (directory, 0700), ==, 0);
  g_assert_true (g_file_set_contents (included, "Included **content**.\n", -1, NULL));
  g_assert_true (g_file_set_contents (empty, "", 0, NULL));
  const char *relative = "@include(included.md)\n\n@include(empty.md)\n\n"
                         "@include(missing.md)\n\n@include(.)\n\nAfter inclusion.\n";
  g_autofree char *html = marker_markdown_to_html (relative, strlen (relative), directory,
    MATHJS_OFF, HIGHLIGHT_OFF, MERMAID_OFF, NULL, NULL, -1);
  g_assert_nonnull (strstr (html, "Included <strong>content</strong>"));
  g_assert_nonnull (strstr (html, "After inclusion."));
  g_autofree char *absolute = g_strdup_printf ("@include(%s)\n", included);
  g_autofree char *absolute_html = marker_markdown_to_html (absolute, strlen (absolute), NULL,
    MATHJS_OFF, HIGHLIGHT_OFF, MERMAID_OFF, NULL, NULL, -1);
  g_assert_nonnull (strstr (absolute_html, "Included <strong>content</strong>"));
  g_autofree char *original_directory = g_get_current_dir ();
  g_assert_cmpint (g_chdir (directory), ==, 0);
  g_autofree char *cwd_html = marker_markdown_to_html (relative, strlen (relative), NULL,
    MATHJS_OFF, HIGHLIGHT_OFF, MERMAID_OFF, NULL, NULL, -1);
  g_assert_cmpint (g_chdir (original_directory), ==, 0);
  g_assert_nonnull (strstr (cwd_html, "Included <strong>content</strong>"));
  g_assert_nonnull (strstr (cwd_html, "After inclusion."));
}

static void
test_invalid_renderer_modes (void)
{
  const char *markdown = "# Safe fallback\n";
  g_autofree char *html = marker_markdown_to_html (markdown, strlen (markdown), NULL,
    (MarkerMathJSMode) -1, (MarkerHighlightMode) -1, (MarkerMermaidMode) -1, NULL, NULL, -1);
  g_assert_nonnull (strstr (html, "Safe fallback"));
  g_assert_null (strstr (html, "renderMathInElement("));
  g_assert_null (strstr (html, "hljs.initHighlightingOnLoad("));
  g_assert_null (strstr (html, "mermaid.initialize("));
}

static void
test_charter_append (void)
{
  char svg[4096] = { 0 };
  svg_header (svg, 320, 200);
  clip_region (svg, 0, 0, 320, 200, "plot");
  rect (svg, 10, 10, 20, 20, "red", "black", 1, "plot");
  line (svg, 0, 0, 10, 20, "blue", 1, NULL);
  regular_text (svg, 20, 20, TXT_LEFT, "Label", NULL);
  circle (svg, 20, 30, 2, "green", NULL);
  svg_footer (svg);
  g_assert_true (g_str_has_prefix (svg, "<svg class=\"charter\""));
  g_assert_nonnull (strstr (svg, "clipPath"));
  g_assert_nonnull (strstr (svg, "<line"));
  g_assert_nonnull (strstr (svg, "Label"));
  g_assert_nonnull (strstr (svg, "<circle"));
  g_assert_true (g_str_has_suffix (svg, "</svg>\n"));
}

static void
test_scientific_export (void)
{
  g_settings_set_boolean (prefs.preview_settings, "gnuplot-toggle", TRUE);
  g_autofree char *input = g_build_filename (fixture, "plot.md", NULL);
  g_autofree char *values = g_build_filename (fixture, "data", "values.csv", NULL);
  g_autofree char *html_path = g_build_filename (fixture, "plot.html", NULL);
  g_autofree char *pdf_path = g_build_filename (fixture, "plot.pdf", NULL);
  g_file_set_contents (values, "0 0\n1 1\n2 4\n", -1, NULL);
  const char *markdown = "# Plot\n\n```gnuplot\nplot 'data/values.csv' using 1:2 with lines\n```\n";
  g_file_set_contents (input, markdown, -1, NULL);
  MarkerWindow *window = new_window ();
  g_autoptr (GFile) root = fixture_file (NULL);
  g_autoptr (GFile) file = fixture_file ("plot.md");
  marker_window_add_project (window, root);
  marker_window_new_editor_from_file (window, file);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  MarkerPreview *preview = marker_editor_get_preview (editor);
  marker_editor_set_view_mode (editor, PREVIEW_ONLY_MODE);
  /* Supersede a render while its worker is starting. Only the new one may complete. */
  marker_editor_refresh_preview (editor);
  marker_editor_jump_to_heading (editor, 18);
  assert_script (preview, "document.querySelector('.gnuplot-preview svg') !== null && "
    "document.querySelector('.gnuplot-error') === null && document.querySelectorAll('#cursor_pos').length === 1");
  g_assert_true (marker_exporter_export (input, html_path));
  g_autofree char *html = NULL;
  g_assert_true (g_file_get_contents (html_path, &html, NULL, NULL));
  g_assert_nonnull (strstr (html, "gnuplot-preview"));
  g_assert_null (strstr (html, "language-gnuplot"));
  g_assert_true (marker_exporter_export (input, pdf_path));
  g_autofree char *pdf = NULL;
  g_assert_true (g_file_get_contents (pdf_path, &pdf, NULL, NULL));
  g_assert_true (g_str_has_prefix (pdf, "%PDF-"));
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  g_remove (input);
  g_remove (values);
  g_remove (html_path);
  g_remove (pdf_path);
}

static void
capture_window (MarkerWindow *window, const char *name)
{
  const char *directory = g_getenv ("MARKER_SCREENSHOT_DIR");
  if (directory == NULL)
    return;
  g_assert_cmpint (g_mkdir_with_parents (directory, 0700), ==, 0);
  g_autoptr (GdkPaintable) paintable = gtk_widget_paintable_new (GTK_WIDGET (window));
  GtkSnapshot *snapshot = gtk_snapshot_new ();
  int width = gtk_widget_get_width (GTK_WIDGET (window));
  int height = gtk_widget_get_height (GTK_WIDGET (window));
  gdk_paintable_snapshot (paintable, snapshot, width, height);
  g_autoptr (GskRenderNode) node = gtk_snapshot_free_to_node (snapshot);
  g_assert_nonnull (node);
  graphene_rect_t viewport = GRAPHENE_RECT_INIT (0, 0, width, height);
  GskRenderer *renderer = gtk_native_get_renderer (GTK_NATIVE (window));
  g_autoptr (GdkTexture) texture = gsk_renderer_render_texture (renderer, node, &viewport);
  g_autofree char *filename = g_strdup_printf ("%s.png", name);
  g_autofree char *path = g_build_filename (directory, filename, NULL);
  g_assert_true (gdk_texture_save_to_png (texture, path));
}

static void
assert_window_color_scheme (MarkerWindow *window)
{
  GdkRGBA background;
  g_assert_true (gtk_style_context_lookup_color (gtk_widget_get_style_context (GTK_WIDGET (window)),
                                                "window_bg_color", &background));
  gboolean dark = (background.red + background.green + background.blue) / 3.0 < .5;
  g_assert_cmpint (dark, ==, adw_style_manager_get_dark (adw_style_manager_get_default ()));
}

static void
test_visual_matrix (void)
{
  g_autofree char *style_path = g_build_filename (fixture, ".marker.css", NULL);
  g_remove (style_path);
  g_settings_set_boolean (prefs.window_settings, "follow-system-theme", FALSE);
  g_settings_set_boolean (prefs.preview_settings, "css-toggle", FALSE);
  g_settings_set_boolean (prefs.preview_settings, "gnuplot-toggle", FALSE);
  g_settings_set_boolean (prefs.editor_settings, "show-line-numbers", FALSE);
  const char *markdown = "# Field notes\n\nA focused space for notebooks, nested research, and scientific Markdown.\n\n"
    "## Wind speed\n\nEach curve reports a median of ten-minute values. Linked data stays beside the notebook.\n\n"
    "### Operating states\n\n- Standstill\n- Operational\n- Unclassified\n\n"
    "## Run the analysis\n\n```python\nuv run scripts/distributions.py\n```\n\n"
    "## Results\n\nKeep **important findings** close to their source.\n";
  MarkerWindow *window = new_window ();
  g_autoptr (GFile) root = fixture_file (NULL);
  marker_window_add_project (window, root);
  marker_window_new_editor (window);
  MarkerEditor *editor = marker_window_get_active_editor (window);
  MarkerProject *project = marker_editor_get_project (editor);
  marker_project_set_name (project, "Research");
  marker_project_set_icon_mode (project, MARKER_PROJECT_ICON_FIRST);
  marker_project_set_color (project, "#4cbd92");
  marker_source_view_set_text (marker_editor_get_source_view (editor), markdown, -1);
  marker_editor_set_view_mode (editor, FORMATTED_MODE);
  activate (window, "outline");
  const int widths[] = { 1440, 900, 500 };
  const char *sizes[] = { "wide", "medium", "narrow" };
  for (guint dark = 0; dark < 2; dark++)
    {
      marker_prefs_set_use_dark_theme (dark);
      marker_editor_apply_prefs (editor);
      for (guint i = 0; i < G_N_ELEMENTS (widths); i++)
        {
          gtk_window_set_default_size (GTK_WINDOW (window), widths[i], 800);
          settle ();
          assert_window_color_scheme (window);
          g_autofree char *name = g_strdup_printf ("%s-%s", dark ? "dark" : "light", sizes[i]);
          capture_window (window, name);
          if (i > 0)
            {
              activate (window, "sidebar");
              g_autofree char *pages = g_strdup_printf ("%s-%s-pages", dark ? "dark" : "light", sizes[i]);
              capture_window (window, pages);
              activate (window, "outline");
              g_autofree char *outline = g_strdup_printf ("%s-%s-outline", dark ? "dark" : "light", sizes[i]);
              capture_window (window, outline);
            }
        }
    }
  gtk_window_set_default_size (GTK_WINDOW (window), 1200, 800);
  marker_editor_set_view_mode (editor, PREVIEW_ONLY_MODE);
  marker_editor_refresh_preview (editor);
  g_autoptr (GError) error = NULL;
  g_assert_true (marker_preview_wait_ready (marker_editor_get_preview (editor), &error));
  settle ();
  capture_window (window, "dark-preview");
  marker_prefs_show_window ();
  settle ();
  AdwDialog *preferences = adw_application_window_get_visible_dialog (ADW_APPLICATION_WINDOW (window));
  g_assert_nonnull (preferences);
  capture_window (window, "dark-preferences");
  adw_dialog_close (preferences);
  settle ();
  g_settings_set_boolean (prefs.window_settings, "follow-system-theme", TRUE);
  settle ();
  assert_window_color_scheme (window);
  gtk_window_destroy (GTK_WINDOW (window));
  settle ();
  marker_prefs_set_use_dark_theme (FALSE);
}

static void
remove_fixture (GFile *file)
{
  g_autoptr (GFileEnumerator) entries = g_file_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
  GFileInfo *info;
  while (entries != NULL && (info = g_file_enumerator_next_file (entries, NULL, NULL)) != NULL)
    {
      g_autoptr (GFile) child = g_file_get_child (file, g_file_info_get_name (info));
      remove_fixture (child);
      g_object_unref (info);
    }
  g_file_delete (file, NULL, NULL);
}

int
main (int argc, char **argv)
{
  g_test_init (&argc, &argv, NULL);
  fixture = g_dir_make_tmp ("marker-ui-notebook-XXXXXX", NULL);
  g_setenv ("MARKER_STATE_DIR", fixture, TRUE);
  g_autofree char *profile = g_build_filename (fixture, ".profile", NULL);
  g_setenv ("XDG_CACHE_HOME", profile, TRUE);
  g_setenv ("XDG_DATA_HOME", profile, TRUE);
  g_setenv ("XDG_CONFIG_HOME", profile, TRUE);
  if (!gtk_init_check ())
    return 77;
  gtk_source_init ();
  spelling_init ();
  app = GTK_APPLICATION (adw_application_new ("com.github.fabiocolacio.marker.Tests", G_APPLICATION_NON_UNIQUE));
  g_assert_true (g_application_register (G_APPLICATION (app), NULL, NULL));
  marker_init ();
  g_autofree char *notes = g_build_filename (fixture, "notes.md", NULL);
  g_autofree char *nested_dir = g_build_filename (fixture, "data", NULL);
  g_autofree char *nested = g_build_filename (nested_dir, "nested.md", NULL);
  g_autofree char *style = g_build_filename (fixture, ".marker.css", NULL);
  g_autofree char *paper = g_build_filename (nested_dir, "paper.svg", NULL);
  g_mkdir (nested_dir, 0700);
  g_file_set_contents (notes, "# Notebook\n\n## First Section\n\nA useful paragraph.\n", -1, NULL);
  g_file_set_contents (nested, "# Nested Page\n", -1, NULL);
  g_file_set_contents (style, "body { color: #123456; }", -1, NULL);
  g_file_set_contents (paper, "<svg xmlns='http://www.w3.org/2000/svg' width='8' height='8'><rect width='8' height='8' fill='white'/></svg>", -1, NULL);
  g_test_add_func ("/window/navigation", test_navigation);
  g_test_add_func ("/window/notebook-remove", test_notebook_remove);
  g_test_add_func ("/window/pages-files-search", test_page_files_search);
  g_test_add_func ("/window/default-typography", test_default_typography);
  g_test_add_func ("/window/document-surface", test_document_surface);
  g_test_add_func ("/window/spell-check", test_spell_check);
  g_test_add_func ("/window/rich-editing", test_rich_editing);
  g_test_add_func ("/window/rich-pointer", test_rich_pointer);
  g_test_add_func ("/window/rich-failures-refresh", test_rich_failures_refresh);
  g_test_add_func ("/window/notebook-styles", test_notebook_styles);
  g_test_add_func ("/window/preview-preferences", test_preview_preferences);
  g_test_add_func ("/renderer/charter-append", test_charter_append);
  g_test_add_func ("/renderer/include-files", test_include_files);
  g_test_add_func ("/renderer/invalid-modes", test_invalid_renderer_modes);
  g_test_add_func ("/window/scientific-export", test_scientific_export);
  g_test_add_func ("/window/visual-matrix", test_visual_matrix);
  int result = g_test_run ();
  marker_session_clear ();
  g_autoptr (GFile) fixture_root = g_file_new_for_path (fixture);
  remove_fixture (fixture_root);
  g_free (fixture);
  g_clear_object (&app);
  gtk_source_finalize ();
  return result;
}
