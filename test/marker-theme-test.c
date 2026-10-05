/* Native regression check; requires a desktop session and installed app data.
 * Run from the repository root after `meson compile -C build`:
 *
 * mkdir -p build/theme-schemas
 * cp data/com.github.fabiocolacio.marker.gschema.xml build/theme-schemas/
 * glib-compile-schemas --strict build/theme-schemas
 * objcopy --redefine-sym main=marker_application_main \
 *   build/marker.p/src_marker.c.o build/marker-theme-main.o
 * objects=()
 * for object in build/marker.p/*.o; do
 *   [[ "$object" == build/marker.p/src_marker.c.o ]] || objects+=("$object")
 * done
 * cc -Isrc test/marker-theme-test.c build/marker-theme-main.o "${objects[@]}" \
 *   -o build/marker-theme-test $(pkg-config --cflags --libs gtk+-3.0 \
 *   gtksourceview-4 webkit2gtk-4.1 libsoup-3.0 gtkspell3-3.0) -lm
 * GSETTINGS_SCHEMA_DIR="$PWD/build/theme-schemas" NO_AT_BRIDGE=1 ./build/marker-theme-test
 */

#include "marker.h"
#include "marker-prefs.h"
#include "marker-window.h"
#include "marker-exporter.h"

extern GtkApplication *app;

static MarkerWindow *windows[2];
static MarkerEditor *editors[3];
static gboolean dirty[3];
static const gchar *document = "# Theme check\n\nA [link](https://example.com) and `code`.\n\n$$x^2+y^2=z^2$$\n\n```c\nint value = 1;\n```\n";
static GMainLoop *preview_loop;
static gboolean expected_dark;
static gboolean check_fonts;
static gboolean expect_plot;

static GtkWidget *
find_widget (GtkWidget *widget, const gchar *name)
{
  if (g_strcmp0 (gtk_buildable_get_name (GTK_BUILDABLE (widget)), name) == 0)
    return widget;
  if (GTK_IS_CONTAINER (widget)) {
    GList *children = gtk_container_get_children (GTK_CONTAINER (widget));
    GtkWidget *found = NULL;
    for (GList *item = children; item && !found; item = item->next)
      found = find_widget (item->data, name);
    g_list_free (children);
    return found;
  }
  return NULL;
}

static void
apply_and_check (const gchar *scheme_id)
{
  for (guint i = 0; i < G_N_ELEMENTS (windows); i++)
    marker_window_apply_prefs (windows[i]);

  for (guint i = 0; i < G_N_ELEMENTS (editors); i++) {
    MarkerSourceView *view = marker_editor_get_source_view (editors[i]);
    GtkSourceBuffer *buffer = GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (view)));
    GtkSourceStyleScheme *scheme = gtk_source_buffer_get_style_scheme (buffer);
    g_assert_nonnull (scheme);
    g_assert_cmpstr (gtk_source_style_scheme_get_id (scheme), ==, scheme_id);
    g_autofree gchar *text = marker_source_view_get_text (view, FALSE);
    g_assert_cmpstr (text, ==, document);
    g_assert_cmpint (marker_source_view_get_cursor_position (view), ==, 6);
    g_assert_cmpint (marker_editor_has_unsaved_changes (editors[i]), ==, dirty[i]);
    g_assert_false (gtk_text_buffer_get_modified (GTK_TEXT_BUFFER (buffer)));
  }
}

static void
preview_checked (GObject *object, GAsyncResult *result, gpointer data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value = webkit_web_view_evaluate_javascript_finish (
    WEBKIT_WEB_VIEW (object), result, &error);
  g_assert_no_error (error);
  g_autofree gchar *text = jsc_value_to_string (value);
  g_assert_nonnull (strstr (text, expected_dark ? "\"dark\":true" : "\"dark\":false"));
  g_assert_nonnull (strstr (text, expected_dark ? "rgb(29, 29, 29)" : "rgb(255, 255, 255)"));
  if (check_fonts) {
    g_print ("Font check: %s\n", text);
    g_assert_nonnull (strstr (text, "\"fonts\":true"));
  }
  g_main_loop_quit (preview_loop);
}

static void
preview_loaded (WebKitWebView *preview, WebKitLoadEvent event, gpointer data)
{
  if (event == WEBKIT_LOAD_FINISHED)
    webkit_web_view_evaluate_javascript (
      preview,
      "JSON.stringify({dark:matchMedia('(prefers-color-scheme:dark)').matches,"
      "body:getComputedStyle(document.body).backgroundColor,"
      "families:['h1,h2','.katex .mathit','pre code','body'].map(s=>"
      "document.querySelector(s)?getComputedStyle(document.querySelector(s)).fontFamily:null),"
      "fonts:!!document.querySelector('.katex .mathit') && "
      "getComputedStyle(document.querySelector('h1,h2')).fontFamily.includes('Serif') && "
      "getComputedStyle(document.querySelector('.katex .mathit')).fontFamily.includes('Serif') && "
      "getComputedStyle(document.querySelector('pre code')).fontFamily.includes('Monospace') && "
      "getComputedStyle(document.body).fontFamily.includes('Sans')})",
      -1, NULL, NULL, NULL, preview_checked, NULL);
}

static gboolean
preview_timeout (gpointer data)
{
  g_error ("Preview did not finish within 15 seconds");
  return G_SOURCE_REMOVE;
}

static void
check_preview (gboolean dark)
{
  expected_dark = dark;
  preview_loop = g_main_loop_new (NULL, FALSE);
  MarkerPreview *preview = marker_editor_get_preview (editors[0]);
  gulong handler = g_signal_connect (preview, "load-changed", G_CALLBACK (preview_loaded), NULL);
  guint timeout = g_timeout_add_seconds (15, preview_timeout, NULL);
  marker_editor_refresh_preview (editors[0]);
  g_main_loop_run (preview_loop);
  g_source_remove (timeout);
  g_signal_handler_disconnect (preview, handler);
  g_main_loop_unref (preview_loop);
}

static void
plot_checked (GObject *object, GAsyncResult *result, gpointer data)
{
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (object), result, &error);
  g_assert_no_error (error);
  g_autofree gchar *text = jsc_value_to_string (value);
  g_print ("Gnuplot check: %s\n", text);
  g_assert_cmpstr (text, ==, expect_plot ? "true" : "false");
  g_main_loop_quit (preview_loop);
}

static void
plot_loaded (WebKitWebView *preview, WebKitLoadEvent event, gpointer data)
{
  if (event == WEBKIT_LOAD_FINISHED)
    g_main_loop_quit (preview_loop);
}

static void
check_gnuplot (gboolean enabled)
{
  expect_plot = enabled;
  MarkerPreview *preview = marker_editor_get_preview (editors[2]);
  preview_loop = g_main_loop_new (NULL, FALSE);
  gulong handler = enabled
    ? g_signal_connect_swapped (preview, "gnuplot-render-complete", G_CALLBACK (g_main_loop_quit), preview_loop)
    : g_signal_connect (preview, "load-changed", G_CALLBACK (plot_loaded), NULL);
  guint timeout = g_timeout_add_seconds (15, preview_timeout, NULL);
  marker_preview_render_markdown (preview, "```gnuplot\nplot sin(x)\n```\n", "GitHub2.css", NULL, -1);
  g_main_loop_run (preview_loop);
  g_signal_handler_disconnect (preview, handler);
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview),
    "!!document.querySelector('.gnuplot-preview svg')", -1, NULL, NULL, NULL, plot_checked, NULL);
  g_main_loop_run (preview_loop);
  g_source_remove (timeout);
  g_main_loop_unref (preview_loop);
}

int
main (int argc, char **argv)
{
  g_setenv ("GSETTINGS_BACKEND", "memory", TRUE);
  gtk_init (&argc, &argv);
  gtk_source_init ();
  app = gtk_application_new ("com.github.fabiocolacio.marker.theme-test", G_APPLICATION_NON_UNIQUE);
  g_assert_true (g_application_register (G_APPLICATION (app), NULL, NULL));
  marker_prefs_load ();
  marker_prefs_set_default_view_mode (DUAL_PANE_MODE);
  marker_prefs_set_syntax_theme ("classic");
  marker_prefs_set_use_syntax_theme (TRUE);
  marker_prefs_set_css_theme ("GitHub2.css");
  marker_prefs_set_use_css_theme (TRUE);
  marker_prefs_set_use_mathjs (FALSE);
  marker_prefs_set_use_mermaid (FALSE);
  marker_prefs_set_use_highlight (FALSE);

  g_autofree gchar *output_dir = g_dir_make_tmp ("marker-theme-test-XXXXXX", NULL);
  g_assert_nonnull (output_dir);
  g_autofree gchar *input = g_build_filename (output_dir, "document.md", NULL);
  g_assert_true (g_file_set_contents (input, document, -1, NULL));

  GtkSourceStyleSchemeManager *manager = gtk_source_style_scheme_manager_get_default ();
  g_auto (GStrv) builtin_paths = g_strdupv ((gchar **) gtk_source_style_scheme_manager_get_search_path (manager));
  windows[0] = marker_window_new (app);
  editors[0] = marker_window_get_active_editor (windows[0]);
  marker_window_new_editor (windows[0]);
  editors[1] = marker_window_get_active_editor (windows[0]);
  windows[1] = marker_window_new (app);
  editors[2] = marker_window_get_active_editor (windows[1]);
  for (guint i = 0; i < G_N_ELEMENTS (windows); i++)
    gtk_widget_hide (GTK_WIDGET (windows[i]));
  for (guint i = 0; i < G_N_ELEMENTS (editors); i++) {
    MarkerSourceView *view = marker_editor_get_source_view (editors[i]);
    if (i == 0)
      marker_editor_open_file (editors[i], g_file_new_for_path (input));
    else
      marker_source_view_set_text (view, document, strlen (document));
    GtkTextBuffer *buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (view));
    GtkTextIter cursor;
    gtk_text_buffer_get_iter_at_offset (buffer, &cursor, 6);
    gtk_text_buffer_place_cursor (buffer, &cursor);
    marker_source_view_set_modified (view, FALSE);
    dirty[i] = marker_editor_has_unsaved_changes (editors[i]);
  }

  gtk_source_style_scheme_manager_prepend_search_path (manager, "data/styles");
  marker_prefs_set_use_dark_theme (TRUE);
  g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", TRUE, NULL);
  apply_and_check ("marker-dark");
  check_preview (TRUE);

  marker_prefs_set_use_syntax_theme (FALSE);
  marker_prefs_set_use_css_theme (FALSE);
  apply_and_check ("marker-dark");
  check_preview (TRUE);
  marker_prefs_set_use_syntax_theme (TRUE);
  marker_prefs_set_use_css_theme (TRUE);

  marker_prefs_set_syntax_theme ("cobalt");
  apply_and_check ("cobalt");
  marker_prefs_set_syntax_theme ("missing-theme");
  apply_and_check ("marker-dark");
  marker_prefs_set_syntax_theme ("classic");

  gtk_source_style_scheme_manager_set_search_path (manager, builtin_paths);
  gtk_source_style_scheme_manager_force_rescan (manager);
  g_assert_null (gtk_source_style_scheme_manager_get_scheme (manager, "marker-dark"));
  apply_and_check ("oblivion");
  gtk_source_style_scheme_manager_prepend_search_path (manager, "data/styles");
  gtk_source_style_scheme_manager_force_rescan (manager);
  apply_and_check ("marker-dark");

  marker_prefs_set_use_dark_theme (FALSE);
  g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", FALSE, NULL);
  apply_and_check ("classic");
  check_preview (FALSE);
  g_autofree gchar *chosen = marker_prefs_get_syntax_theme ();
  g_assert_cmpstr (chosen, ==, "classic");
  g_autofree gchar *css = marker_prefs_get_css_theme ();
  g_assert_cmpstr (css, ==, "GitHub2.css");

  marker_prefs_show_window ();
  GList *toplevels = gtk_window_list_toplevels ();
  GtkWidget *preferences = NULL;
  for (GList *item = toplevels; item; item = item->next)
    if (find_widget (item->data, "prefs_notebook"))
      preferences = item->data;
  g_list_free (toplevels);
  g_assert_nonnull (preferences);
  g_assert_true (GTK_IS_HEADER_BAR (gtk_window_get_titlebar (GTK_WINDOW (preferences))));
  gtk_widget_hide (preferences);
  GtkWidget *font_button = find_widget (preferences, "editor_font_button");
  gtk_font_chooser_set_font (GTK_FONT_CHOOSER (font_button), "Monospace 14");
  g_signal_emit_by_name (font_button, "font-set");
  g_autofree gchar *editor_font = marker_prefs_get_editor_font ();
  g_assert_cmpstr (editor_font, ==, "Monospace 14");
  for (guint i = 0; i < G_N_ELEMENTS (editors); i++) {
    GtkStyleContext *context = gtk_widget_get_style_context (GTK_WIDGET (marker_editor_get_source_view (editors[i])));
    g_autoptr (PangoFontDescription) font = NULL;
    gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL, "font", &font, NULL);
    g_assert_cmpstr (pango_font_description_get_family (font), ==, "Monospace");
    g_assert_cmpint (pango_font_description_get_size (font), ==, 14 * PANGO_SCALE);
  }
  const gchar *roles[] = {"header", "math", "code", "text"};
  const gchar *fonts[] = {"Serif Bold 20", "Serif 15", "Monospace 12", "Sans 13"};
  for (guint i = 0; i < G_N_ELEMENTS (roles); i++) {
    g_autofree gchar *button_id = g_strdup_printf ("%s_font_button", roles[i]);
    g_autofree gchar *toggle_id = g_strdup_printf ("%s_font_check_button", roles[i]);
    GtkWidget *button = find_widget (preferences, button_id);
    g_assert_false (gtk_widget_get_sensitive (button));
    gtk_font_chooser_set_font (GTK_FONT_CHOOSER (button), fonts[i]);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (find_widget (preferences, toggle_id)), TRUE);
    g_assert_true (gtk_widget_get_sensitive (button));
    g_autofree gchar *stored = marker_prefs_get_preview_font (roles[i]);
    g_assert_cmpstr (stored, ==, fonts[i]);
  }
  marker_prefs_set_use_mathjs (TRUE);
  check_fonts = TRUE;
  check_preview (FALSE);
  check_fonts = FALSE;
  for (guint i = 0; i < G_N_ELEMENTS (roles); i++) {
    g_autofree gchar *toggle_id = g_strdup_printf ("%s_font_check_button", roles[i]);
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (find_widget (preferences, toggle_id)), FALSE);
    g_assert_null (marker_prefs_get_preview_font (roles[i]));
  }
  marker_prefs_set_use_mathjs (FALSE);
  GtkToggleButton *gnuplot = GTK_TOGGLE_BUTTON (find_widget (preferences, "gnuplot_check_button"));
  g_assert_true (gtk_toggle_button_get_active (gnuplot));
  gtk_toggle_button_set_active (gnuplot, FALSE);
  g_assert_false (marker_prefs_get_use_gnuplot ());
  check_gnuplot (FALSE);
  gtk_toggle_button_set_active (gnuplot, TRUE);
  g_assert_true (marker_prefs_get_use_gnuplot ());
  check_gnuplot (TRUE);

  g_autoptr (GSettings) desktop = g_settings_new ("org.gnome.desktop.interface");
  g_settings_set_string (desktop, "color-scheme", "prefer-dark");
  GtkToggleButton *follow = GTK_TOGGLE_BUTTON (find_widget (preferences, "follow_system_theme_check_button"));
  gtk_toggle_button_set_active (follow, TRUE);
  g_assert_true (marker_prefs_get_use_dark_theme ());
  g_assert_false (gtk_widget_get_sensitive (find_widget (preferences, "enable_dark_mode_check_button")));
  check_preview (TRUE);
  g_settings_set_string (desktop, "color-scheme", "prefer-light");
  g_assert_false (marker_prefs_get_use_dark_theme ());
  check_preview (FALSE);
  gtk_toggle_button_set_active (follow, FALSE);
  g_assert_false (marker_prefs_get_use_dark_theme ());
  g_assert_true (gtk_widget_get_sensitive (find_widget (preferences, "enable_dark_mode_check_button")));
  apply_and_check ("classic");
  gtk_widget_destroy (preferences);

  g_autofree gchar *plot_input = g_build_filename (output_dir, "plot.md", NULL);
  g_autofree gchar *plot_html = g_build_filename (output_dir, "plot.html", NULL);
  g_assert_true (g_file_set_contents (plot_input, "```gnuplot\nplot sin(x)\n```\n", -1, NULL));
  marker_prefs_set_use_gnuplot (FALSE);
  g_assert_true (marker_exporter_export (plot_input, plot_html));
  g_autofree gchar *plot_source = NULL;
  g_assert_true (g_file_get_contents (plot_html, &plot_source, NULL, NULL));
  g_assert_nonnull (strstr (plot_source, "class=\"language-gnuplot\""));
  marker_prefs_set_use_gnuplot (TRUE);
  g_assert_true (marker_exporter_export (plot_input, plot_html));
  g_autofree gchar *plot_rendered = NULL;
  g_assert_true (g_file_get_contents (plot_html, &plot_rendered, NULL, NULL));
  g_assert_nonnull (strstr (plot_rendered, "<svg"));
  g_assert_null (strstr (plot_rendered, "class=\"language-gnuplot\""));

  g_autofree gchar *light_html = g_build_filename (output_dir, "light.html", NULL);
  g_autofree gchar *dark_html = g_build_filename (output_dir, "dark.html", NULL);
  g_autofree gchar *pdf = g_build_filename (output_dir, "dark.pdf", NULL);
  g_assert_true (marker_exporter_export (input, light_html));
  marker_prefs_set_use_dark_theme (TRUE);
  g_object_set (gtk_settings_get_default (), "gtk-application-prefer-dark-theme", TRUE, NULL);
  g_assert_true (marker_exporter_export (input, dark_html));
  g_assert_true (marker_exporter_export (input, pdf));
  g_autofree gchar *light = NULL;
  g_autofree gchar *dark = NULL;
  g_assert_true (g_file_get_contents (light_html, &light, NULL, NULL));
  g_assert_true (g_file_get_contents (dark_html, &dark, NULL, NULL));
  g_assert_cmpstr (dark, ==, light);
  g_assert_null (strstr (dark, "prefers-color-scheme"));
  g_print ("PASS: themes, all editors, content/cursor, font controls, system-theme changes, gnuplot toggle, HTML/PDF exports\n");
  g_print ("Export artifacts: %s\n", output_dir);
  return 0;
}
