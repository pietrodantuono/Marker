/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-render-style.h"
#include "marker-prefs.h"

static char *
css_quote (const char *text)
{
  GString *quoted = g_string_new (NULL);
  for (const char *p = text; *p != '\0'; p++)
    {
      if (*p == '"' || *p == '\\')
        g_string_append_c (quoted, '\\');
      if (*p == '<')
        g_string_append (quoted, "\\3c ");
      else
        g_string_append_c (quoted, *p == '\n' || *p == '\r' ? ' ' : *p);
    }
  return g_string_free (quoted, FALSE);
}

static char *
css_family (const char *family)
{
  const char *generic[] = { "serif", "sans-serif", "monospace", "cursive", "fantasy",
    "system-ui", "ui-serif", "ui-sans-serif", "ui-monospace", "ui-rounded", "emoji", "math", "fangsong" };
  if (g_ascii_strcasecmp (family, "sans") == 0)
    return g_strdup ("sans-serif");
  for (guint i = 0; i < G_N_ELEMENTS (generic); i++)
    if (g_ascii_strcasecmp (family, generic[i]) == 0)
      return g_strdup (generic[i]);
  g_autofree char *escaped = css_quote (family);
  return g_strdup_printf ("\"%s\"", escaped);
}

static void
append_sheet (GString *css, const char *path, const char *layer)
{
  if (path == NULL || !g_file_test (path, G_FILE_TEST_IS_REGULAR))
    return;
  g_autofree char *uri = g_filename_to_uri (path, NULL, NULL);
  if (uri != NULL)
    g_string_append_printf (css, "@import url(\"%s?marker=%" G_GINT64_FORMAT "\") layer(%s);\n",
                            uri, g_get_monotonic_time (), layer);
}

static void
append_font (GString *css, const char *chosen, const char *selector)
{
  if (chosen == NULL || *chosen == '\0')
    return;
  g_autoptr (PangoFontDescription) font = pango_font_description_from_string (chosen);
  const char *family = pango_font_description_get_family (font);
  if (family == NULL)
    return;
  g_autofree char *quoted = css_family (family);
  PangoStyle style = pango_font_description_get_style (font);
  g_string_append_printf (css, "%s { font-family: %s; font-weight: %d; font-style: %s;",
    selector, quoted, pango_font_description_get_weight (font),
    style == PANGO_STYLE_ITALIC ? "italic" : style == PANGO_STYLE_OBLIQUE ? "oblique" : "normal");
  double size = (double) pango_font_description_get_size (font) / PANGO_SCALE;
  if (size > 0)
    {
      char number[G_ASCII_DTOSTR_BUF_SIZE];
      g_ascii_dtostr (number, sizeof number, size);
      g_string_append_printf (css, "font-size: %s%s;", number,
        pango_font_description_get_size_is_absolute (font) ? "px" : "pt");
    }
  g_string_append (css, " }\n");
}

char *
marker_render_style_build (const char *theme, const char *notebook_folder, const char *document_folder)
{
  g_autoptr (GString) css = g_string_new (
    "@layer marker-base, marker-theme, scidown-document, marker-preferences, marker-notebook;\n");
  g_autofree char *base = g_build_filename (COMMON_DIR, "scidown.css", NULL);
  g_autofree char *theme_path = theme != NULL && *theme != '\0'
    ? (g_path_is_absolute (theme) ? g_strdup (theme) : g_build_filename (STYLES_DIR, theme, NULL)) : NULL;
  g_autofree char *notebook = notebook_folder != NULL ? g_build_filename (notebook_folder, ".marker.css", NULL) : NULL;
  append_sheet (css, base, "marker-base");
  append_sheet (css, theme_path, "marker-theme");
  append_sheet (css, notebook, "marker-notebook");
  g_string_append (css, "@layer marker-base {\n");
  g_autoptr (GBytes) base_css = g_resources_lookup_data (
    "/com/github/fabiocolacio/marker/styles/marker-preview-base.css", 0, NULL);
  if (base_css != NULL)
    g_string_append_len (css, g_bytes_get_data (base_css, NULL), g_bytes_get_size (base_css));
  g_autofree char *prose = g_settings_get_string (prefs.editor_settings, "prose-font");
  g_autofree char *code = g_settings_get_string (prefs.editor_settings, "code-font");
  append_font (css, prose, "body");
  append_font (css, code, "pre, code");
  g_string_append (css, "\n}\n@layer marker-preferences {\n");
  g_string_append_printf (css, "@media screen { body { max-width: %upx; } }\n",
                          g_settings_get_uint (prefs.editor_settings, "writing-width"));
  if (marker_prefs_get_use_dark_theme ())
    {
      g_autoptr (GBytes) dark = g_resources_lookup_data (
        "/com/github/fabiocolacio/marker/styles/marker-preview-dark.css", 0, NULL);
      if (dark != NULL)
        g_string_append_len (css, g_bytes_get_data (dark, NULL), g_bytes_get_size (dark));
    }
  const char *roles[] = { "header", "math", "code", "text" };
  const char *selectors[] = {
    "h1, h2, h3, h4, h5, h6, h7", ".katex, .MathJax, .MathJax_Display, .MathJax_SVG, math",
    "pre, code, pre code, pre code *", "body, p, blockquote, li, td, th"
  };
  for (guint i = 0; i < G_N_ELEMENTS (roles); i++)
    {
      g_autofree char *font = marker_prefs_get_preview_font (roles[i]);
      append_font (css, font, selectors[i]);
      if (i == 1 && font != NULL && *font != '\0')
        {
          g_autoptr (PangoFontDescription) description = pango_font_description_from_string (font);
          const char *family = pango_font_description_get_family (description);
          if (family != NULL)
            {
              g_autofree char *quoted = css_family (family);
              g_string_append_printf (css, ".katex *, .MathJax * { font-family: %s; }\n", quoted);
            }
        }
    }
  g_string_append (css, "}\n");
  g_autoptr (GString) header = g_string_new (NULL);
  if (document_folder != NULL)
    {
      g_autofree char *directory = g_strconcat (document_folder, G_DIR_SEPARATOR_S, NULL);
      g_autofree char *uri = g_filename_to_uri (directory, NULL, NULL);
      if (uri != NULL)
        {
          g_autofree char *escaped = g_markup_escape_text (uri, -1);
          g_string_append_printf (header, "<base href=\"%s\">\n", escaped);
        }
    }
  g_string_append_printf (header, "<style>\n%s</style>\n", css->str);
  return g_string_free (g_steal_pointer (&header), FALSE);
}
