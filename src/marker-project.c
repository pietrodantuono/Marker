/* marker-project.c
 * Copyright (C) 2026 Marker contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-project.h"
#include <math.h>
#include <string.h>

struct _MarkerProject
{
  GObject parent_instance;
  GFile *root;
  char *uri;
  char *name;
  char *text;
  char *icon_name;
  char *custom_text;
  char *color;
  MarkerProjectIconMode icon_mode;
  gboolean missing;
  GFileMonitor *styles_monitor;
  guint styles_source;
};

enum { PROP_0, PROP_NAME, PROP_TEXT, PROP_ICON_NAME, PROP_ICON_MODE,
       PROP_CUSTOM_TEXT, PROP_COLOR, PROP_ROOT, PROP_MISSING, N_PROPS };
static GParamSpec *properties[N_PROPS];

G_DEFINE_TYPE (MarkerProject, marker_project, G_TYPE_OBJECT)
static guint styles_changed_signal;

static gboolean
styles_changed (gpointer data)
{
  MarkerProject *self = MARKER_PROJECT (data);
  self->styles_source = 0;
  g_signal_emit (self, styles_changed_signal, 0);
  return G_SOURCE_REMOVE;
}

static void
styles_monitor_changed (GFileMonitor *monitor, GFile *file, GFile *other,
                        GFileMonitorEvent event, MarkerProject *self)
{
  g_autofree char *name = g_file_get_basename (file);
  g_autofree char *other_name = other != NULL ? g_file_get_basename (other) : NULL;
  if (g_strcmp0 (name, ".marker.css") != 0 && g_strcmp0 (other_name, ".marker.css") != 0)
    return;
  g_clear_handle_id (&self->styles_source, g_source_remove);
  self->styles_source = g_timeout_add_full (G_PRIORITY_LOW, 180, styles_changed, self, NULL);
}

static const char *const colors[] = {
  "#3584e4", "#2ec27e", "#c88800", "#e66100", "#e01b24", "#9141ac", "#5e5c64"
};
static const char *const color_names[] = {
  "blue", "green", "yellow", "orange", "red", "purple", "slate"
};

static char *
first_chars (const char *name)
{
  return g_utf8_substring (name, 0, MIN (2, g_utf8_strlen (name, -1)));
}

static void
update_text (MarkerProject *self)
{
  g_autoptr (GString) text = g_string_new (NULL);
  if (self->icon_mode == MARKER_PROJECT_ICON_CUSTOM)
    g_string_append (text, self->custom_text != NULL ? self->custom_text : "");
  else if (self->icon_mode == MARKER_PROJECT_ICON_FIRST ||
           self->icon_mode == MARKER_PROJECT_ICON_SYMBOL)
    {
      g_autofree char *first = first_chars (self->name);
      g_string_append (text, first);
    }
  else if (self->icon_mode == MARKER_PROJECT_ICON_CAMEL)
    {
      const char *p = self->name;
      if (*p != '\0')
        {
          g_string_append_unichar (text, g_utf8_get_char (p));
          for (p = g_utf8_next_char (p); *p != '\0'; p = g_utf8_next_char (p))
            if (g_unichar_isupper (g_utf8_get_char (p)))
              {
                g_string_append_unichar (text, g_utf8_get_char (p));
                break;
              }
        }
    }
  else
    {
      g_auto (GStrv) words = g_strsplit_set (self->name,
        self->icon_mode == MARKER_PROJECT_ICON_SNAKE ? "_" : " _-.", -1);
      guint count = 0;
      for (guint i = 0; words[i] != NULL && count < 2; i++)
        if (*words[i] != '\0')
          {
            g_string_append_unichar (text, g_unichar_toupper (g_utf8_get_char (words[i])));
            count++;
          }
    }
  if (g_utf8_strlen (text->str, -1) < 2 && self->icon_mode != MARKER_PROJECT_ICON_CUSTOM)
    {
      g_autofree char *first = first_chars (self->name);
      g_string_assign (text, first);
    }
  if (g_strcmp0 (self->text, text->str) != 0)
    {
      g_free (self->text);
      self->text = g_string_free (g_steal_pointer (&text), FALSE);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_TEXT]);
    }
}

static void
refresh_identity (MarkerProject *self)
{
  g_autofree char *path = g_file_get_path (self->root);
  g_autofree char *canonical = path != NULL ? g_canonicalize_filename (path, NULL) : NULL;
  g_autoptr (GFile) root = canonical != NULL ? g_file_new_for_path (canonical) : g_object_ref (self->root);
  g_set_object (&self->root, root);
  g_free (self->uri);
  self->uri = g_file_get_uri (self->root);
  self->missing = !g_file_query_exists (self->root, NULL);
  g_clear_handle_id (&self->styles_source, g_source_remove);
  g_clear_object (&self->styles_monitor);
  self->styles_monitor = g_file_monitor_directory (self->root, G_FILE_MONITOR_WATCH_MOVES, NULL, NULL);
  if (self->styles_monitor != NULL)
    g_signal_connect_object (self->styles_monitor, "changed", G_CALLBACK (styles_monitor_changed), self, 0);
  if (self->name == NULL)
    self->name = g_file_get_basename (self->root);
  if (self->name == NULL || *self->name == '\0')
    {
      g_free (self->name);
      self->name = g_strdup ("Notebook");
    }
  if (self->color == NULL)
    self->color = g_strdup (colors[g_str_hash (self->uri) % G_N_ELEMENTS (colors)]);
  update_text (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ROOT]);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_MISSING]);
}

static void
marker_project_get_property (GObject *object, guint id, GValue *value, GParamSpec *pspec)
{
  MarkerProject *self = MARKER_PROJECT (object);
  switch (id)
    {
    case PROP_NAME: g_value_set_string (value, self->name); break;
    case PROP_TEXT: g_value_set_string (value, self->text); break;
    case PROP_ICON_NAME: g_value_set_string (value, self->icon_name); break;
    case PROP_ICON_MODE: g_value_set_uint (value, self->icon_mode); break;
    case PROP_CUSTOM_TEXT: g_value_set_string (value, self->custom_text); break;
    case PROP_COLOR: g_value_set_string (value, self->color); break;
    case PROP_ROOT: g_value_set_object (value, self->root); break;
    case PROP_MISSING: g_value_set_boolean (value, self->missing); break;
    default: G_OBJECT_WARN_INVALID_PROPERTY_ID (object, id, pspec);
    }
}

static void
marker_project_dispose (GObject *object)
{
  MarkerProject *self = MARKER_PROJECT (object);
  g_clear_handle_id (&self->styles_source, g_source_remove);
  g_clear_object (&self->styles_monitor);
  g_clear_object (&self->root);
  G_OBJECT_CLASS (marker_project_parent_class)->dispose (object);
}

static void
marker_project_finalize (GObject *object)
{
  MarkerProject *self = MARKER_PROJECT (object);
  g_free (self->uri);
  g_free (self->name);
  g_free (self->text);
  g_free (self->icon_name);
  g_free (self->custom_text);
  g_free (self->color);
  G_OBJECT_CLASS (marker_project_parent_class)->finalize (object);
}

static void
marker_project_class_init (MarkerProjectClass *klass)
{
  styles_changed_signal = g_signal_new ("styles-changed", G_TYPE_FROM_CLASS (klass),
    G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->get_property = marker_project_get_property;
  object_class->dispose = marker_project_dispose;
  object_class->finalize = marker_project_finalize;
  properties[PROP_NAME] = g_param_spec_string ("name", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_TEXT] = g_param_spec_string ("text", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_ICON_NAME] = g_param_spec_string ("icon-name", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_ICON_MODE] = g_param_spec_uint ("icon-mode", NULL, NULL, 0, MARKER_PROJECT_ICON_CUSTOM, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_CUSTOM_TEXT] = g_param_spec_string ("custom-text", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_COLOR] = g_param_spec_string ("color", NULL, NULL, NULL, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_ROOT] = g_param_spec_object ("root", NULL, NULL, G_TYPE_FILE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  properties[PROP_MISSING] = g_param_spec_boolean ("missing", NULL, NULL, FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_properties (object_class, N_PROPS, properties);
}

static void
marker_project_init (MarkerProject *self)
{
}

MarkerProject *
marker_project_new (GFile *root)
{
  g_return_val_if_fail (G_IS_FILE (root), NULL);
  MarkerProject *self = g_object_new (MARKER_TYPE_PROJECT, NULL);
  self->root = g_object_ref (root);
  refresh_identity (self);
  return self;
}

GFile *
marker_project_get_root (MarkerProject *self)
{
  return self->root;
}

const char *
marker_project_get_uri (MarkerProject *self)
{
  return self->uri;
}

const char *
marker_project_get_name (MarkerProject *self)
{
  return self->name;
}

const char *
marker_project_get_text (MarkerProject *self)
{
  return self->text;
}

const char *
marker_project_get_icon_name (MarkerProject *self)
{
  return self->icon_name;
}

const char *
marker_project_get_custom_text (MarkerProject *self)
{
  return self->custom_text;
}

const char *
marker_project_get_color (MarkerProject *self)
{
  return self->color;
}

MarkerProjectIconMode
marker_project_get_icon_mode (MarkerProject *self)
{
  return self->icon_mode;
}

gboolean
marker_project_get_missing (MarkerProject *self)
{
  return self->missing;
}


static void
set_string (MarkerProject *self, char **field, const char *value, guint property)
{
  if (g_strcmp0 (*field, value) == 0)
    return;
  g_free (*field);
  *field = g_strdup (value);
  g_object_notify_by_pspec (G_OBJECT (self), properties[property]);
}

void
marker_project_set_name (MarkerProject *self, const char *name)
{
  g_return_if_fail (name != NULL && *name != '\0' && g_utf8_validate (name, -1, NULL));
  set_string (self, &self->name, name, PROP_NAME);
  update_text (self);
}

void
marker_project_set_icon_name (MarkerProject *self, const char *name)
{
  set_string (self, &self->icon_name, name, PROP_ICON_NAME);
}

void
marker_project_set_custom_text (MarkerProject *self, const char *text)
{
  if (text != NULL && !g_utf8_validate (text, -1, NULL))
    return;
  set_string (self, &self->custom_text, text, PROP_CUSTOM_TEXT);
  update_text (self);
}

void
marker_project_set_icon_mode (MarkerProject *self, MarkerProjectIconMode mode)
{
  if (mode < MARKER_PROJECT_ICON_FIRST || mode > MARKER_PROJECT_ICON_CUSTOM || mode == self->icon_mode)
    return;
  self->icon_mode = mode;
  update_text (self);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_ICON_MODE]);
}

void
marker_project_set_color (MarkerProject *self, const char *color)
{
  if (color == NULL)
    return;
  for (guint i = 0; i < G_N_ELEMENTS (colors); i++)
    if (g_str_equal (color, color_names[i]))
      color = colors[i];
  if (strlen (color) != 7 || color[0] != '#')
    return;
  for (guint i = 1; i < 7; i++)
    if (!g_ascii_isxdigit (color[i]))
      return;
  g_autofree char *normalized = g_ascii_strdown (color, -1);
  set_string (self, &self->color, normalized, PROP_COLOR);
}

const char *
marker_project_get_foreground (MarkerProject *self)
{
  double luminance = 0;
  static const double weights[] = { .2126, .7152, .0722 };
  for (guint i = 0; i < 3; i++)
    {
      double channel = (g_ascii_xdigit_value (self->color[1 + 2*i]) * 16 +
                        g_ascii_xdigit_value (self->color[2 + 2*i])) / 255.;
      luminance += weights[i] * (channel <= .04045 ? channel / 12.92 : pow ((channel + .055) / 1.055, 2.4));
    }
  return (luminance + .05) / .05 >= 1.05 / (luminance + .05) ? "#000000" : "#ffffff";
}

void
marker_project_copy_appearance (MarkerProject *self, MarkerProject *source)
{
  g_object_freeze_notify (G_OBJECT (self));
  marker_project_set_name (self, source->name);
  marker_project_set_icon_name (self, source->icon_name);
  marker_project_set_custom_text (self, source->custom_text);
  marker_project_set_icon_mode (self, source->icon_mode);
  marker_project_set_color (self, source->color);
  g_object_thaw_notify (G_OBJECT (self));
}

void
marker_project_relocate (MarkerProject *self, GFile *root)
{
  g_return_if_fail (G_IS_FILE (root));
  g_set_object (&self->root, root);
  refresh_identity (self);
}

gboolean
marker_project_contains (MarkerProject *self, GFile *file)
{
  return file != NULL && (g_file_equal (self->root, file) || g_file_has_prefix (file, self->root));
}

GFile *
marker_project_ensure_stylesheet (MarkerProject *self, GError **error)
{
  g_autoptr (GFile) file = g_file_get_child (self->root, ".marker.css");
  GFileType type = g_file_query_file_type (file, G_FILE_QUERY_INFO_NONE, NULL);
  if (type == G_FILE_TYPE_REGULAR)
    return g_steal_pointer (&file);
  if (type != G_FILE_TYPE_UNKNOWN)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_REGULAR_FILE,
                           ".marker.css must be a regular file.");
      return NULL;
    }
  g_autoptr (GFileOutputStream) stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, error);
  if (stream == NULL)
    return NULL;
  const char *initial = "/* Notebook styles override the global preview theme. */\n";
  if (!g_output_stream_write_all (G_OUTPUT_STREAM (stream), initial, strlen (initial), NULL, NULL, error) ||
      !g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, error))
    return NULL;
  return g_steal_pointer (&file);
}
