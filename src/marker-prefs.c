/* marker-prefs.c
 *
 * Copyright (C) 2017-2026 Marker contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-prefs.h"

#include <adwaita.h>
#include <gtksourceview/gtksource.h>

#include "marker.h"
#include "marker-window.h"

MarkerPrefs prefs;

static GSettings *desktop_settings;
static AdwDialog *preferences_dialog;
static GtkCssProvider *zorin_theme_provider;

static gboolean
has_existing_profile (void)
{
  GSettings *settings[] = {
    prefs.editor_settings,
    prefs.preview_settings,
    prefs.window_settings
  };
  const char *schema_ids[] = {
    "com.github.fabiocolacio.marker.preferences.editor",
    "com.github.fabiocolacio.marker.preferences.preview",
    "com.github.fabiocolacio.marker.preferences.window"
  };
  GSettingsSchemaSource *source = g_settings_schema_source_get_default ();

  for (guint i = 0; i < G_N_ELEMENTS (settings); i++)
    {
      GSettingsSchema *schema = g_settings_schema_source_lookup (source,
                                                                 schema_ids[i],
                                                                 TRUE);
      g_auto (GStrv) keys = NULL;

      if (schema == NULL)
        continue;
      keys = g_settings_schema_list_keys (schema);
      for (guint j = 0; keys[j] != NULL; j++)
        {
          g_autoptr (GVariant) value = NULL;

          if (g_str_equal (keys[j], "profile-initialized"))
            continue;
          value = g_settings_get_user_value (settings[i], keys[j]);
          if (value != NULL)
            {
              g_settings_schema_unref (schema);
              return TRUE;
            }
        }
      g_settings_schema_unref (schema);
    }

  return FALSE;
}

static void
apply_zorin_theme (AdwStyleManager *manager)
{
  g_autofree char *theme_path = NULL;
  gboolean zorin_theme = FALSE;

  /* Zorin's theme-path loader imports widget rules as well as colors. Keep
   * Libadwaita's widget styling and effective scheme instead of importing another
   * full GTK theme. Other themes and high contrast retain their native styling. */
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (manager), "theme-path") != NULL &&
      !adw_style_manager_get_high_contrast (manager))
    {
      g_object_get (manager, "theme-path", &theme_path, NULL);
      if (theme_path != NULL)
        {
          g_autofree char *directory = g_path_get_dirname (theme_path);
          g_autofree char *theme = g_path_get_basename (directory);
          zorin_theme = g_str_has_prefix (theme, "Zorin") &&
            (g_str_has_suffix (theme, "-Dark") || g_str_has_suffix (theme, "-Light"));
        }
    }

  if (zorin_theme)
    {
      if (zorin_theme_provider == NULL)
        {
          zorin_theme_provider = gtk_css_provider_new ();
          gtk_style_context_add_provider_for_display (gdk_display_get_default (),
            GTK_STYLE_PROVIDER (zorin_theme_provider), GTK_STYLE_PROVIDER_PRIORITY_THEME + 1);
        }
      g_autofree char *stylesheet = g_strdup_printf (
        "@import url('resource:///org/gnome/Adwaita/styles/base.css');"
        "@import url('resource:///org/gnome/Adwaita/styles/defaults-%s.css');",
        adw_style_manager_get_dark (manager) ? "dark" : "light");
      gtk_css_provider_load_from_string (zorin_theme_provider, stylesheet);
    }
  else if (zorin_theme_provider != NULL)
    {
      gtk_style_context_remove_provider_for_display (gdk_display_get_default (),
                                                     GTK_STYLE_PROVIDER (zorin_theme_provider));
      g_clear_object (&zorin_theme_provider);
    }
}

static void
apply_color_scheme (void)
{
  AdwStyleManager *manager = adw_style_manager_get_default ();

  if (marker_prefs_get_follow_system_theme ())
    adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_DEFAULT);
  else if (g_settings_get_boolean (prefs.window_settings, "enable-dark-mode"))
    adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_DARK);
  else
    adw_style_manager_set_color_scheme (manager, ADW_COLOR_SCHEME_FORCE_LIGHT);
  apply_zorin_theme (manager);
}

gboolean
marker_prefs_get_use_dark_theme (void)
{
  if (marker_prefs_get_follow_system_theme ())
    return adw_style_manager_get_dark (adw_style_manager_get_default ());
  return g_settings_get_boolean (prefs.window_settings, "enable-dark-mode");
}

void
marker_prefs_set_use_dark_theme (gboolean state)
{
  g_settings_set_boolean (prefs.window_settings, "enable-dark-mode", state);
  apply_color_scheme ();
}

gboolean
marker_prefs_get_follow_system_theme (void)
{
  return g_settings_get_boolean (prefs.window_settings, "follow-system-theme");
}


gchar *
marker_prefs_get_editor_font (void)
{
  char *font = g_settings_get_string (prefs.editor_settings, "font");

  if (*font != '\0')
    return font;
  g_free (font);
  return g_settings_get_string (desktop_settings, "monospace-font-name");
}


#define UINT_GETTER(Name, Settings, Key) \
  guint marker_prefs_get_##Name (void) { return g_settings_get_uint (prefs.Settings, Key); }

#define BOOL_GETTER(Name, Settings, Key) \
  gboolean marker_prefs_get_##Name (void) { return g_settings_get_boolean (prefs.Settings, Key); }

#define STRING_GETTER(Name, Settings, Key) \
  char * marker_prefs_get_##Name (void) { return g_settings_get_string (prefs.Settings, Key); }

UINT_GETTER (window_width, window_settings, "window-width")
void
marker_prefs_set_window_width (guint value)
{
  g_settings_set_uint (prefs.window_settings, "window-width", value);
}

UINT_GETTER (window_height, window_settings, "window-height")
void
marker_prefs_set_window_height (guint value)
{
  g_settings_set_uint (prefs.window_settings, "window-height", value);
}

UINT_GETTER (editor_pane_width, window_settings, "editor-pane-width")
void
marker_prefs_set_editor_pane_width (guint value)
{
  g_settings_set_uint (prefs.window_settings, "editor-pane-width", value);
}

UINT_GETTER (tab_width, editor_settings, "tab-width")
UINT_GETTER (right_margin_position, editor_settings, "show-right-margin-position")

BOOL_GETTER (show_sidebar, window_settings, "show-sidebar")

BOOL_GETTER (replace_tabs, editor_settings, "replace-tabs")
BOOL_GETTER (auto_indent, editor_settings, "auto-indent")

BOOL_GETTER (spell_check, editor_settings, "spell-check")
BOOL_GETTER (show_line_numbers, editor_settings, "show-line-numbers")
BOOL_GETTER (highlight_current_line, editor_settings, "highlight-current-line")
BOOL_GETTER (wrap_text, editor_settings, "wrap-text")
BOOL_GETTER (show_right_margin, editor_settings, "show-right-margin")
BOOL_GETTER (use_css_theme, preview_settings, "css-toggle")
void
marker_prefs_set_use_css_theme (gboolean value)
{
  g_settings_set_boolean (prefs.preview_settings, "css-toggle", value);
}

BOOL_GETTER (use_mathjs, preview_settings, "mathjs-toggle")
BOOL_GETTER (use_highlight, preview_settings, "highlight-toggle")
BOOL_GETTER (use_mermaid, preview_settings, "mermaid-toggle")
BOOL_GETTER (use_gnuplot, preview_settings, "gnuplot-toggle")
BOOL_GETTER (use_charter, preview_settings, "charter-toggle")

STRING_GETTER (syntax_theme, editor_settings, "syntax-theme")
STRING_GETTER (spell_check_language, editor_settings, "spell-check-lang")
STRING_GETTER (css_theme, preview_settings, "css-theme")
void
marker_prefs_set_css_theme (const char * value)
{
  g_settings_set_string (prefs.preview_settings, "css-theme", value);
}

STRING_GETTER (highlight_theme, preview_settings, "highlight-theme")



gdouble
marker_prefs_get_zoom_level (void)
{
  return g_settings_get_double (prefs.preview_settings, "preview-zoom-level");
}

void
marker_prefs_set_zoom_level (gdouble value)
{
  g_settings_set_double (prefs.preview_settings, "preview-zoom-level", value);
}

gchar *
marker_prefs_get_preview_font (const gchar *role)
{
  g_autofree char *toggle_key = g_strdup_printf ("override-%s-font", role);
  g_autofree char *font_key = g_strdup_printf ("%s-font", role);

  if (!g_settings_get_boolean (prefs.preview_settings, toggle_key))
    return NULL;
  return g_settings_get_string (prefs.preview_settings, font_key);
}



MarkerViewMode
marker_prefs_get_default_view_mode (void)
{
  return (MarkerViewMode) g_settings_get_enum (prefs.window_settings, "view-mode");
}

void
marker_prefs_set_default_view_mode (MarkerViewMode view_mode)
{
  g_settings_set_enum (prefs.window_settings, "view-mode", view_mode);
}

MarkerMathBackEnd
marker_prefs_get_math_backend (void)
{
  return (MarkerMathBackEnd) g_settings_get_enum (prefs.preview_settings, "math-backend");
}

void
marker_prefs_set_math_backend (MarkerMathBackEnd backend)
{
  g_settings_set_enum (prefs.preview_settings, "math-backend", backend);
}

static void
theme_setting_changed_cb (GSettings *settings,
                          gchar     *key,
                          gpointer   user_data)
{
  apply_color_scheme ();
  GtkApplication *application = marker_get_app ();
  if (application == NULL)
    return;
  for (GList *item = gtk_application_get_windows (application);
       item != NULL; item = item->next)
    if (MARKER_IS_WINDOW (item->data))
      marker_window_apply_prefs (MARKER_WINDOW (item->data));
}

static void
preference_changed_cb (GSettings *settings,
                       gchar     *key,
                       gpointer   user_data)
{
  GtkApplication *application = marker_get_app ();

  if (application == NULL ||
      (settings == prefs.preview_settings &&
       g_str_equal (key, "preview-zoom-level")))
    return;
  for (GList *item = gtk_application_get_windows (application);
       item != NULL; item = item->next)
    if (MARKER_IS_WINDOW (item->data))
      marker_window_apply_prefs (MARKER_WINDOW (item->data));
}

static void
system_theme_changed_cb (AdwStyleManager *manager,
                         GParamSpec      *pspec,
                         gpointer         user_data)
{
  apply_zorin_theme (manager);
  if (marker_prefs_get_follow_system_theme ())
    preference_changed_cb (prefs.window_settings, "follow-system-theme", NULL);
}

void
marker_prefs_load (void)
{
  gboolean existing;

  if (prefs.editor_settings != NULL)
    return;

  prefs.editor_settings = g_settings_new ("com.github.fabiocolacio.marker.preferences.editor");
  prefs.preview_settings = g_settings_new ("com.github.fabiocolacio.marker.preferences.preview");
  prefs.window_settings = g_settings_new ("com.github.fabiocolacio.marker.preferences.window");
  desktop_settings = g_settings_new ("org.gnome.desktop.interface");

  existing = has_existing_profile ();
  if (!g_settings_get_boolean (prefs.window_settings, "profile-initialized"))
    {
      if (!existing)
        g_settings_set_enum (prefs.window_settings, "view-mode", FORMATTED_MODE);
      g_settings_set_boolean (prefs.window_settings, "profile-initialized", TRUE);
    }

  g_signal_connect (prefs.window_settings, "changed::enable-dark-mode",
                    G_CALLBACK (theme_setting_changed_cb), NULL);
  g_signal_connect (prefs.window_settings, "changed::follow-system-theme",
                    G_CALLBACK (theme_setting_changed_cb), NULL);
  g_signal_connect (prefs.editor_settings, "changed",
                    G_CALLBACK (preference_changed_cb), NULL);
  g_signal_connect (prefs.preview_settings, "changed",
                    G_CALLBACK (preference_changed_cb), NULL);
  g_signal_connect (adw_style_manager_get_default (), "notify::dark",
                    G_CALLBACK (system_theme_changed_cb), NULL);
  g_signal_connect (adw_style_manager_get_default (), "notify::high-contrast",
                    G_CALLBACK (system_theme_changed_cb), NULL);
  if (g_object_class_find_property (G_OBJECT_GET_CLASS (adw_style_manager_get_default ()), "theme-path") != NULL)
    g_signal_connect (adw_style_manager_get_default (), "notify::theme-path",
                      G_CALLBACK (system_theme_changed_cb), NULL);
  apply_color_scheme ();
}

static AdwPreferencesGroup *
add_group (AdwPreferencesPage *page,
           const char         *title)
{
  AdwPreferencesGroup *group = ADW_PREFERENCES_GROUP (adw_preferences_group_new ());
  adw_preferences_group_set_title (group, title);
  adw_preferences_page_add (page, group);
  return group;
}

static AdwSwitchRow *
add_switch (AdwPreferencesGroup *group,
            GSettings           *settings,
            const char          *key,
            const char          *title,
            const char          *subtitle)
{
  AdwSwitchRow *row = ADW_SWITCH_ROW (adw_switch_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  if (subtitle != NULL)
    adw_action_row_set_subtitle (ADW_ACTION_ROW (row), subtitle);
  adw_preferences_group_add (group, GTK_WIDGET (row));
  g_settings_bind (settings, key, row, "active", G_SETTINGS_BIND_DEFAULT);
  return row;
}

static AdwSpinRow *
add_spin (AdwPreferencesGroup *group,
          GSettings           *settings,
          const char          *key,
          const char          *title,
          double               lower,
          double               upper,
          double               step)
{
  AdwSpinRow *row = ADW_SPIN_ROW (adw_spin_row_new_with_range (lower, upper, step));
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  adw_preferences_group_add (group, GTK_WIDGET (row));
  g_settings_bind (settings, key, row, "value", G_SETTINGS_BIND_DEFAULT);
  return row;
}

static gboolean
monospace_font_filter (gpointer item,
                       gpointer user_data)
{
  return PANGO_IS_FONT_FAMILY (item) && pango_font_family_is_monospace (item);
}

typedef struct
{
  GSettings *settings;
  char *key;
} FontBinding;

static void
font_binding_free (FontBinding *binding)
{
  g_free (binding->key);
  g_free (binding);
}

static void
font_binding_closure_free (gpointer  data,
                           GClosure *closure)
{
  font_binding_free (data);
}

static void
font_changed_cb (GtkFontDialogButton *button,
                 GParamSpec          *pspec,
                 FontBinding         *binding)
{
  const PangoFontDescription *description = gtk_font_dialog_button_get_font_desc (button);
  g_autofree char *font = pango_font_description_to_string (description);
  g_settings_set_string (binding->settings, binding->key, font);
}

static GtkWidget *
add_font (AdwPreferencesGroup *group,
          GSettings           *settings,
          const char          *key,
          const char          *title,
          gboolean             monospace)
{
  AdwActionRow *row = ADW_ACTION_ROW (adw_action_row_new ());
  GtkFontDialog *dialog = gtk_font_dialog_new ();
  GtkWidget *button = gtk_font_dialog_button_new (dialog);
  g_autofree char *font = g_settings_get_string (settings, key);
  g_autoptr (PangoFontDescription) description = NULL;
  FontBinding *binding = g_new0 (FontBinding, 1);

  if (*font == '\0' && settings == prefs.editor_settings && g_str_equal (key, "font"))
    {
      g_free (g_steal_pointer (&font));
      font = marker_prefs_get_editor_font ();
    }
  description = pango_font_description_from_string (font);

  if (monospace)
    {
      GtkFilter *filter = GTK_FILTER (gtk_custom_filter_new (monospace_font_filter, NULL, NULL));
      gtk_font_dialog_set_filter (dialog, filter);
      g_object_unref (filter);
    }

  gtk_font_dialog_button_set_font_desc (GTK_FONT_DIALOG_BUTTON (button), description);
  gtk_font_dialog_button_set_use_font (GTK_FONT_DIALOG_BUTTON (button), TRUE);
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  adw_action_row_add_suffix (row, button);
  adw_action_row_set_activatable_widget (row, button);
  adw_preferences_group_add (group, GTK_WIDGET (row));

  binding->settings = settings;
  binding->key = g_strdup (key);
  g_signal_connect_data (button, "notify::font-desc", G_CALLBACK (font_changed_cb),
                         binding, font_binding_closure_free, 0);
  return GTK_WIDGET (row);
}

static AdwComboRow *
add_combo (AdwPreferencesGroup *group,
           const char *const   *items,
           const char          *title,
           guint                selected)
{
  GtkStringList *model = gtk_string_list_new (items);
  AdwComboRow *row = ADW_COMBO_ROW (adw_combo_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (row), title);
  adw_combo_row_set_model (row, G_LIST_MODEL (model));
  adw_combo_row_set_selected (row, selected);
  adw_preferences_group_add (group, GTK_WIDGET (row));
  g_object_unref (model);
  return row;
}

static void
view_mode_changed_cb (AdwComboRow *row,
                      GParamSpec  *pspec,
                      gpointer     user_data)
{
  marker_prefs_set_default_view_mode (adw_combo_row_get_selected (row));
}

static void
math_backend_changed_cb (AdwComboRow *row,
                         GParamSpec  *pspec,
                         gpointer     user_data)
{
  marker_prefs_set_math_backend (adw_combo_row_get_selected (row));
}

static void
add_preview_font_override (AdwPreferencesGroup *group,
                           const char          *role,
                           const char          *title,
                           gboolean             monospace)
{
  g_autofree char *toggle = g_strdup_printf ("override-%s-font", role);
  g_autofree char *key = g_strdup_printf ("%s-font", role);
  AdwSwitchRow *switch_row = add_switch (group, prefs.preview_settings, toggle,
                                         title, "Use a custom font in the rendered preview");
  GtkWidget *font_row = add_font (group, prefs.preview_settings, key, "Font", monospace);

  g_object_bind_property (switch_row, "active", font_row, "sensitive",
                          G_BINDING_SYNC_CREATE);
}

static void
theme_file_finished (GObject *object, GAsyncResult *result, gpointer data)
{
  g_autoptr (AdwActionRow) row = data;
  g_autoptr (GFile) file = gtk_file_dialog_open_finish (GTK_FILE_DIALOG (object), result, NULL);
  if (file == NULL)
    return;
  g_autofree char *path = g_file_get_path (file);
  if (path != NULL)
    {
      marker_prefs_set_css_theme (path);
      adw_action_row_set_subtitle (row, path);
    }
}

static void
choose_theme_file (GtkButton *button, AdwActionRow *row)
{
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  GtkFileFilter *filter = gtk_file_filter_new ();
  g_autoptr (GListStore) filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  gtk_file_filter_add_pattern (filter, "*.css");
  gtk_file_filter_set_name (filter, "CSS stylesheets");
  g_list_store_append (filters, filter);
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_set_title (dialog, "Choose Preview Theme");
  gtk_file_dialog_open (dialog, GTK_WINDOW (gtk_widget_get_root (GTK_WIDGET (button))), NULL,
                        theme_file_finished, g_object_ref (row));
  g_object_unref (filter);
  g_object_unref (dialog);
}

static void
theme_selected (AdwComboRow *row, GParamSpec *pspec, gpointer data)
{
  GtkStringList *model = GTK_STRING_LIST (adw_combo_row_get_model (row));
  const char *theme = gtk_string_list_get_string (model, adw_combo_row_get_selected (row));
  if (theme != NULL)
    marker_prefs_set_css_theme (theme);
}

static void
theme_setting_changed (GSettings *settings, const char *key, AdwComboRow *row)
{
  g_autofree char *current = g_settings_get_string (settings, key);
  GtkStringList *themes = GTK_STRING_LIST (adw_combo_row_get_model (row));
  guint count = g_list_model_get_n_items (G_LIST_MODEL (themes));
  guint selected = count;
  for (guint i = 0; i < count; i++)
    if (g_str_equal (current, gtk_string_list_get_string (themes, i)))
      { selected = i; break; }
  if (selected == count)
    gtk_string_list_append (themes, current);
  adw_combo_row_set_selected (row, selected);
}

static void
add_theme_chooser (AdwPreferencesGroup *group)
{
  g_autofree char *current = marker_prefs_get_css_theme ();
  g_autoptr (GtkStringList) themes = gtk_string_list_new (NULL);
  g_autoptr (GDir) directory = g_dir_open (STYLES_DIR, 0, NULL);
  const char *name;
  guint selected = GTK_INVALID_LIST_POSITION;
  while (directory != NULL && (name = g_dir_read_name (directory)) != NULL)
    if (g_str_has_suffix (name, ".css"))
      {
        if (g_str_equal (current, name))
          selected = g_list_model_get_n_items (G_LIST_MODEL (themes));
        gtk_string_list_append (themes, name);
      }
  if (selected == GTK_INVALID_LIST_POSITION)
    {
      selected = g_list_model_get_n_items (G_LIST_MODEL (themes));
      gtk_string_list_append (themes, current);
    }
  AdwComboRow *combo = ADW_COMBO_ROW (adw_combo_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (combo), "Preview theme");
  adw_combo_row_set_model (combo, G_LIST_MODEL (themes));
  adw_combo_row_set_selected (combo, selected);
  g_signal_connect (combo, "notify::selected", G_CALLBACK (theme_selected), NULL);
  g_signal_connect_object (prefs.preview_settings, "changed::css-theme", G_CALLBACK (theme_setting_changed), combo, 0);
  adw_preferences_group_add (group, GTK_WIDGET (combo));
  AdwActionRow *custom = ADW_ACTION_ROW (adw_action_row_new ());
  adw_preferences_row_set_title (ADW_PREFERENCES_ROW (custom), "Custom stylesheet");
  adw_action_row_set_subtitle (custom, current);
  g_settings_bind (prefs.preview_settings, "css-theme", custom, "subtitle", G_SETTINGS_BIND_GET);
  GtkWidget *button = gtk_button_new_with_label ("Choose…");
  gtk_widget_set_valign (button, GTK_ALIGN_CENTER);
  adw_action_row_add_suffix (custom, button);
  adw_action_row_set_activatable_widget (custom, button);
  g_signal_connect (button, "clicked", G_CALLBACK (choose_theme_file), custom);
  adw_preferences_group_add (group, GTK_WIDGET (custom));
}

void
marker_prefs_show_window (void)
{
  GtkWindow *parent;
  AdwPreferencesPage *editor_page;
  AdwPreferencesPage *preview_page;
  AdwPreferencesPage *window_page;
  AdwPreferencesGroup *group;
  AdwComboRow *combo;
  static const char *const view_modes[] = {
    "Editor Only", "Preview Only", "Dual Pane", "Dual Window", "Formatted Markdown", NULL
  };
  static const char *const math_backends[] = { "KaTeX", "MathJax", NULL };

  if (preferences_dialog != NULL)
    {
      adw_dialog_present (preferences_dialog,
                          GTK_WIDGET (gtk_application_get_active_window (marker_get_app ())));
      return;
    }

  preferences_dialog = adw_preferences_dialog_new ();
  g_object_add_weak_pointer (G_OBJECT (preferences_dialog), (gpointer *) &preferences_dialog);
  adw_dialog_set_title (preferences_dialog, "Preferences");
  adw_dialog_set_content_width (preferences_dialog, 680);
  adw_dialog_set_content_height (preferences_dialog, 720);

  editor_page = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
  adw_preferences_page_set_title (editor_page, "Editor");
  adw_preferences_page_set_icon_name (editor_page, "document-edit-symbolic");
  group = add_group (editor_page, "Text");
  add_font (group, prefs.editor_settings, "font", "Editor font", TRUE);
  add_switch (group, prefs.editor_settings, "show-line-numbers", "Line numbers", NULL);
  add_switch (group, prefs.editor_settings, "highlight-current-line", "Highlight current line", NULL);
  add_switch (group, prefs.editor_settings, "wrap-text", "Wrap long lines", NULL);
  add_switch (group, prefs.editor_settings, "spell-check", "Spell checking", "Uses libspelling and installed dictionaries");
  group = add_group (editor_page, "Formatted Markdown");
  add_font (group, prefs.editor_settings, "prose-font", "Prose font", FALSE);
  add_font (group, prefs.editor_settings, "code-font", "Code font", TRUE);
  add_spin (group, prefs.editor_settings, "writing-width", "Writing width", 520, 1400, 20);
  add_spin (group, prefs.editor_settings, "line-spacing", "Line spacing", 0, 16, 1);

  preview_page = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
  adw_preferences_page_set_title (preview_page, "Preview");
  adw_preferences_page_set_icon_name (preview_page, "view-reveal-symbolic");
  group = add_group (preview_page, "Rendering");
  add_switch (group, prefs.preview_settings, "css-toggle", "Use preview theme", NULL);
  add_theme_chooser (group);
  add_switch (group, prefs.preview_settings, "mathjs-toggle", "Render mathematics", NULL);
  combo = add_combo (group, math_backends, "Math renderer", marker_prefs_get_math_backend ());
  g_signal_connect (combo, "notify::selected", G_CALLBACK (math_backend_changed_cb), NULL);
  add_switch (group, prefs.preview_settings, "highlight-toggle", "Highlight code", NULL);
  add_switch (group, prefs.preview_settings, "mermaid-toggle", "Enable Mermaid", NULL);
  add_switch (group, prefs.preview_settings, "gnuplot-toggle", "Enable gnuplot", "Render gnuplot fenced blocks and linked data files");
  add_switch (group, prefs.preview_settings, "charter-toggle", "Enable Charter", "Render Charter fenced plots");
  group = add_group (preview_page, "Font overrides");
  add_preview_font_override (group, "header", "Headings", FALSE);
  add_preview_font_override (group, "math", "Mathematics", FALSE);
  add_preview_font_override (group, "code", "Code blocks", TRUE);
  add_preview_font_override (group, "text", "Body text", FALSE);

  window_page = ADW_PREFERENCES_PAGE (adw_preferences_page_new ());
  adw_preferences_page_set_title (window_page, "Window");
  adw_preferences_page_set_icon_name (window_page, "preferences-system-windows-symbolic");
  group = add_group (window_page, "Appearance");
  add_switch (group, prefs.window_settings, "follow-system-theme", "Follow system theme", NULL);
  add_switch (group, prefs.window_settings, "enable-dark-mode", "Use dark appearance", "Used when system theme following is off");
  group = add_group (window_page, "Default view");
  combo = add_combo (group, view_modes, "New documents open in",
                     marker_prefs_get_default_view_mode ());
  g_signal_connect (combo, "notify::selected", G_CALLBACK (view_mode_changed_cb), NULL);

  adw_preferences_dialog_add (ADW_PREFERENCES_DIALOG (preferences_dialog), editor_page);
  adw_preferences_dialog_add (ADW_PREFERENCES_DIALOG (preferences_dialog), preview_page);
  adw_preferences_dialog_add (ADW_PREFERENCES_DIALOG (preferences_dialog), window_page);

  parent = gtk_application_get_active_window (marker_get_app ());
  adw_dialog_present (preferences_dialog, GTK_WIDGET (parent));
}
