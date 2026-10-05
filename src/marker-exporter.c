/* marker-exporter.c
 *
 * Copyright (C) 2017-2026 Marker contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-exporter.h"

#include <adwaita.h>
#include <glib/gstdio.h>
#include <string.h>
#include <unistd.h>

#include "marker-editor.h"
#include "marker-markdown.h"
#include "marker-prefs.h"
#include "marker-preview.h"

static gboolean
has_suffix (const char *value,
            const char *suffix)
{
  gsize value_length = strlen (value);
  gsize suffix_length = strlen (suffix);
  return value_length >= suffix_length &&
         g_ascii_strcasecmp (value + value_length - suffix_length, suffix) == 0;
}

MarkerExportFormat
marker_exporter_str_to_fmt (const char *value)
{
  if (g_ascii_strcasecmp (value, "PDF") == 0) return PDF;
  if (g_ascii_strcasecmp (value, "RTF") == 0) return RTF;
  if (g_ascii_strcasecmp (value, "ODT") == 0) return ODT;
  if (g_ascii_strcasecmp (value, "DOCX") == 0) return DOCX;
  if (g_ascii_strcasecmp (value, "LATEX") == 0) return LATEX;
  return HTML;
}

static gboolean
export_html (const char  *markdown,
             gsize        length,
             const char  *base_folder,
             const char  *document_path,
             const char  *stylesheet,
             const char  *notebook_folder,
             const char  *outfile,
             GError     **error)
{
  g_autofree char *final_html = marker_markdown_to_html (
    markdown, length, (char *) base_folder,
    marker_prefs_get_use_mathjs () ? MATHJS_NET : MATHJS_OFF,
    marker_prefs_get_use_highlight () ? HIGHLIGHT_NET : HIGHLIGHT_OFF,
    marker_prefs_get_use_mermaid () ? MERMAID_NET : MERMAID_OFF,
    stylesheet, notebook_folder, -1);

  if (final_html == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Unable to render the Markdown document.");
      return FALSE;
    }
  if (!marker_prefs_get_use_gnuplot () ||
      strstr (final_html, "class=\"language-gnuplot\"") == NULL)
    return g_file_set_contents (outfile, final_html, -1, error);

  g_autofree char *staging_html = marker_markdown_to_html (
    markdown, length, (char *) base_folder, MATHJS_OFF, HIGHLIGHT_OFF,
    MERMAID_OFF, stylesheet, notebook_folder, -1);
  if (staging_html == NULL)
    {
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Unable to prepare the gnuplot export.");
      return FALSE;
    }
  return marker_preview_export_html (staging_html, final_html, document_path,
                                     outfile, error);
}

static gboolean
export_with_pandoc (const char  *markdown,
                    const char  *stylesheet,
                    const char  *outfile,
                    GError     **error)
{
  g_autofree char *temporary = NULL;
  gint descriptor = g_file_open_tmp ("marker-export-XXXXXX.md", &temporary, error);
  g_autoptr (GSubprocess) process = NULL;
  const char *argv_with_css[] = { "pandoc", "-s", "-c", stylesheet,
                                  "-o", outfile, temporary, NULL };
  const char *argv[] = { "pandoc", "-s", "-o", outfile, temporary, NULL };
  gboolean result;

  if (descriptor < 0)
    return FALSE;
  close (descriptor);
  if (!g_file_set_contents (temporary, markdown, -1, error))
    return FALSE;
  process = g_subprocess_newv (stylesheet != NULL && *stylesheet != '\0'
                                 ? argv_with_css : argv,
                               G_SUBPROCESS_FLAGS_STDERR_PIPE, error);
  if (process == NULL)
    return FALSE;
  result = g_subprocess_wait_check (process, NULL, error);
  g_unlink (temporary);
  return result;
}

static gboolean
export_document (const char    *markdown,
                 const char    *source_path,
                 const char    *notebook_folder,
                 const char    *outfile,
                 GError       **error)
{
  g_autofree char *theme = marker_prefs_get_css_theme ();
  g_autofree char *stylesheet = marker_prefs_get_use_css_theme ()
                                  ? (g_path_is_absolute (theme) ? g_strdup (theme) : g_build_filename (STYLES_DIR, theme, NULL))
                                  : NULL;
  g_autofree char *base_folder = source_path != NULL
                                   ? g_path_get_dirname (source_path)
                                   : g_strdup (notebook_folder != NULL ? notebook_folder : g_get_home_dir ());
  gsize length = strlen (markdown);

  if (has_suffix (outfile, ".html") || has_suffix (outfile, ".htm"))
    return export_html (markdown, length, base_folder, source_path, stylesheet,
                        notebook_folder, outfile, error);

  if (has_suffix (outfile, ".tex"))
    {
      marker_markdown_to_latex_file (
        markdown, length, base_folder,
        marker_prefs_get_use_mathjs () ? MATHJS_NET : MATHJS_OFF,
        marker_prefs_get_use_highlight () ? HIGHLIGHT_NET : HIGHLIGHT_OFF,
        marker_prefs_get_use_mermaid () ? MERMAID_NET : MERMAID_OFF,
        outfile);
      return g_file_test (outfile, G_FILE_TEST_EXISTS);
    }

  if (has_suffix (outfile, ".pdf"))
    {
      metadata *meta = marker_markdown_metadata (markdown, length);
      enum scidown_paper_size paper_size = meta != NULL ? meta->paper_size : A4PAPER;
      GtkPageOrientation orientation =
        meta != NULL && meta->doc_class == CLASS_BEAMER
          ? GTK_PAGE_ORIENTATION_LANDSCAPE : GTK_PAGE_ORIENTATION_PORTRAIT;
      g_autoptr (MarkerPreview) owned_preview = NULL;

      if (meta != NULL && meta->doc_class == CLASS_BEAMER &&
          paper_size != B43 && paper_size != B169)
        paper_size = B43;
      if (meta != NULL)
        free_meta (meta);
      owned_preview = g_object_ref_sink (marker_preview_new ());
      MarkerPreview *preview = owned_preview;
      marker_preview_render_markdown (preview, markdown, stylesheet,
                                      source_path, notebook_folder, -1);
      if (!marker_preview_print_pdf (preview, outfile, paper_size, orientation))
        {
          g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                               "Unable to create the PDF document.");
          return FALSE;
        }
      return TRUE;
    }

  return export_with_pandoc (markdown, stylesheet, outfile, error);
}

static void
show_export_error (MarkerWindow *window,
                   const GError *error)
{
  AdwAlertDialog *dialog = ADW_ALERT_DIALOG (adw_alert_dialog_new (
    "Export failed", error != NULL ? error->message : "Unknown export error"));
  adw_alert_dialog_add_response (dialog, "close", "Close");
  adw_dialog_present (ADW_DIALOG (dialog), GTK_WIDGET (window));
}

typedef struct
{
  MarkerWindow *window;
  MarkerEditor *editor;
} ExportRequest;

static void
export_finished_cb (GObject *source, GAsyncResult *result, gpointer user_data)
{
  ExportRequest *request = user_data;
  g_autoptr (MarkerWindow) window = request->window;
  g_autoptr (MarkerEditor) editor = request->editor;
  g_free (request);
  g_autoptr (GError) error = NULL;
  g_autoptr (GFile) destination = gtk_file_dialog_save_finish (GTK_FILE_DIALOG (source), result, &error);
  if (destination == NULL)
    return;
  g_autofree char *outfile = g_file_get_path (destination);
  g_autofree char *markdown = marker_source_view_get_text (marker_editor_get_source_view (editor));
  GFile *source_file = marker_editor_get_file (editor);
  g_autofree char *source_path = source_file != NULL ? g_file_get_path (source_file) : NULL;
  MarkerProject *project = marker_editor_get_project (editor);
  g_autofree char *notebook = project != NULL ? g_file_get_path (marker_project_get_root (project)) : NULL;
  if (outfile == NULL || !export_document (markdown, source_path, notebook, outfile, &error))
    show_export_error (window, error);
}

void
marker_exporter_show_export_dialog (MarkerWindow *window)
{
  MarkerEditor *editor = marker_window_get_active_editor (window);
  if (editor == NULL)
    return;
  ExportRequest *request = g_new0 (ExportRequest, 1);
  request->window = g_object_ref (window);
  request->editor = g_object_ref (editor);
  GtkFileDialog *dialog = gtk_file_dialog_new ();
  GListStore *filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
  const struct { const char *name; const char *pattern; } formats[] = {
    { "HTML", "*.html" }, { "PDF", "*.pdf" }, { "LaTeX", "*.tex" },
    { "Word", "*.docx" }, { "OpenDocument", "*.odt" }, { "RTF", "*.rtf" }
  };

  gtk_file_dialog_set_title (dialog, "Export Document");
  gtk_file_dialog_set_initial_name (dialog, "document.html");
  for (guint i = 0; i < G_N_ELEMENTS (formats); i++)
    {
      GtkFileFilter *filter = gtk_file_filter_new ();
      gtk_file_filter_set_name (filter, formats[i].name);
      gtk_file_filter_add_pattern (filter, formats[i].pattern);
      g_list_store_append (filters, filter);
      if (i == 0)
        gtk_file_dialog_set_default_filter (dialog, filter);
      g_object_unref (filter);
    }
  gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
  gtk_file_dialog_save (dialog, GTK_WINDOW (window), NULL, export_finished_cb, request);
  g_object_unref (filters);
  g_object_unref (dialog);
}

gboolean
marker_exporter_export (const gchar *infile,
                        const gchar *outfile)
{
  g_autofree char *markdown = NULL;
  gsize length = 0;
  g_autoptr (GError) error = NULL;

  g_return_val_if_fail (infile != NULL && outfile != NULL, FALSE);
  if (!g_file_get_contents (infile, &markdown, &length, &error))
    {
      g_printerr ("Unable to read Markdown input: %s\n", error->message);
      return FALSE;
    }
  g_autofree char *folder = g_path_get_dirname (infile);
  g_autofree char *notebook = NULL;
  for (g_autofree char *candidate = g_canonicalize_filename (folder, NULL); candidate != NULL; )
    {
      g_autofree char *style = g_build_filename (candidate, ".marker.css", NULL);
      if (g_file_test (style, G_FILE_TEST_IS_REGULAR))
        {
          notebook = g_strdup (candidate);
          break;
        }
      g_autofree char *parent = g_path_get_dirname (candidate);
      if (g_str_equal (candidate, parent))
        break;
      g_free (candidate);
      candidate = g_steal_pointer (&parent);
    }
  if (!export_document (markdown, infile, notebook, outfile, &error))
    {
      g_printerr ("Export failed: %s\n", error->message);
      return FALSE;
    }
  return TRUE;
}
