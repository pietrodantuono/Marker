/* marker-editor.c
 *
 * Copyright (C) 2017-2026 Marker contributors
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "marker-editor.h"

#include <adwaita.h>
#include <glib/gi18n.h>

#include "marker-formatted.h"
#include "marker-prefs.h"
#include "marker-rich-view.h"

struct _MarkerEditor
{
  GtkBox parent_instance;

  MarkerSourceView *source_view;
  MarkerPreview *preview;
  MarkerRichView *rich_view;
  GtkWidget *source_scroller;
  GtkWidget *content;
  GtkWidget *format_bar;
  GtkButton *render_button;
  gboolean render_inline;
  GtkSearchBar *search_bar;
  GtkSearchEntry *search_entry;
  GtkWidget *replace_row;
  GtkEntry *replace_entry;
  GtkWidget *preview_window;
  GtkDropDown *heading_chooser;
  gboolean heading_sync;
  GtkPaned *paned;

  GFile *file;
  MarkerProject *project;
  GFileMonitor *monitor;
  MarkerViewMode view_mode;
  guint refresh_source;
  gboolean loading;
  gboolean focus_mode;
};

G_DEFINE_TYPE (MarkerEditor, marker_editor, GTK_TYPE_BOX)

enum
{
  TITLE_CHANGED,
  SUBTITLE_CHANGED,
  LAST_SIGNAL
};

static guint editor_signals[LAST_SIGNAL];

static void marker_editor_update_layout (MarkerEditor *self);
static void refresh_preview (MarkerEditor *self, gboolean force_preview);

static void
sync_inline_mode (MarkerEditor *self)
{
  gboolean rendering = self->view_mode == FORMATTED_MODE && self->render_inline;
  marker_rich_view_set_enabled (self->rich_view, rendering);
  marker_source_view_set_formatted (self->source_view, rendering);
  gtk_button_set_label (self->render_button, self->render_inline ? _("Source") : _("Render"));
  gtk_widget_set_tooltip_text (GTK_WIDGET (self->render_button), self->render_inline
    ? _("Show page source and pause inline rendering") : _("Render page elements"));
}

static void
detach_widget (GtkWidget *widget)
{
  GtkWidget *parent = gtk_widget_get_parent (widget);

  if (parent == NULL)
    return;
  if (GTK_IS_BOX (parent))
    gtk_box_remove (GTK_BOX (parent), widget);
  else if (GTK_IS_PANED (parent))
    {
      if (gtk_paned_get_start_child (GTK_PANED (parent)) == widget)
        gtk_paned_set_start_child (GTK_PANED (parent), NULL);
      else if (gtk_paned_get_end_child (GTK_PANED (parent)) == widget)
        gtk_paned_set_end_child (GTK_PANED (parent), NULL);
    }
  else if (ADW_IS_WINDOW (parent))
    adw_window_set_content (ADW_WINDOW (parent), NULL);
  else if (GTK_IS_WINDOW (parent))
    gtk_window_set_child (GTK_WINDOW (parent), NULL);
  else
    gtk_widget_unparent (widget);
}

static gboolean
refresh_timeout_cb (gpointer user_data)
{
  MarkerEditor *self = MARKER_EDITOR (user_data);
  self->refresh_source = 0;
  marker_editor_refresh_preview (self);
  return G_SOURCE_REMOVE;
}

static void
queue_preview_refresh (MarkerEditor *self)
{
  g_clear_handle_id (&self->refresh_source, g_source_remove);
  self->refresh_source = g_timeout_add_full (G_PRIORITY_LOW, 180,
                                             refresh_timeout_cb,
                                             g_object_ref (self),
                                             g_object_unref);
}

static void
buffer_changed_cb (GtkTextBuffer *buffer,
                   MarkerEditor  *self)
{
  if (self->loading)
    return;
  queue_preview_refresh (self);
  g_signal_emit (self, editor_signals[TITLE_CHANGED], 0);
}

static void
buffer_modified_cb (GtkTextBuffer *buffer,
                    MarkerEditor  *self)
{
  g_signal_emit (self, editor_signals[TITLE_CHANGED], 0);
}

static void
file_monitor_changed_cb (GFileMonitor      *monitor,
                         GFile             *file,
                         GFile             *other_file,
                         GFileMonitorEvent  event,
                         MarkerEditor      *self)
{
  if (event == G_FILE_MONITOR_EVENT_DELETED)
    g_clear_object (&self->monitor);
}

static void
configure_file_monitor (MarkerEditor *self)
{
  g_autoptr (GError) error = NULL;

  g_clear_object (&self->monitor);
  if (self->file == NULL)
    return;

  self->monitor = g_file_monitor_file (self->file, G_FILE_MONITOR_NONE, NULL, &error);
  if (self->monitor != NULL)
    g_signal_connect (self->monitor, "changed", G_CALLBACK (file_monitor_changed_cb), self);
}

static void
search (MarkerEditor *self,
        gboolean      forward)
{
  GtkSourceSearchContext *context = marker_source_get_search_context (self->source_view);
  GtkSourceSearchSettings *settings = gtk_source_search_context_get_settings (context);
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self));
  GtkTextIter cursor;
  GtkTextIter start;
  GtkTextIter end;
  gboolean wrapped = FALSE;
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  gboolean found;

  gtk_source_search_settings_set_search_text (settings, text);
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    cursor = forward ? end : start;
  else
    gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
  found = forward
            ? gtk_source_search_context_forward (context, &cursor, &start, &end, &wrapped)
            : gtk_source_search_context_backward (context, &cursor, &start, &end, &wrapped);
  if (!found)
    {
      if (forward)
        gtk_text_buffer_get_start_iter (buffer, &cursor);
      else
        gtk_text_buffer_get_end_iter (buffer, &cursor);
      found = forward
                ? gtk_source_search_context_forward (context, &cursor, &start, &end, &wrapped)
                : gtk_source_search_context_backward (context, &cursor, &start, &end, &wrapped);
    }

  if (found)
    {
      gtk_text_buffer_select_range (buffer, &start, &end);
      gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self->source_view), &start, .15, FALSE, 0, 0);
    }
}

static void
replace_cb (GtkWidget    *widget,
            MarkerEditor *self)
{
  GtkSourceSearchContext *context = marker_source_get_search_context (self->source_view);
  GtkSourceSearchSettings *settings = gtk_source_search_context_get_settings (context);
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self));
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));
  const char *replacement = gtk_editable_get_text (GTK_EDITABLE (self->replace_entry));
  GtkTextIter cursor, start, end;

  if (*text == '\0')
    return;
  gtk_source_search_settings_set_search_text (settings, text);
  if (gtk_text_buffer_get_selection_bounds (buffer, &start, &end))
    cursor = start;
  else
    gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
  if (!gtk_source_search_context_forward (context, &cursor, &start, &end, NULL))
    {
      gtk_text_buffer_get_start_iter (buffer, &cursor);
      if (!gtk_source_search_context_forward (context, &cursor, &start, &end, NULL))
        return;
    }
  if (gtk_source_search_context_replace (context, &start, &end, replacement, -1, NULL))
    {
      gtk_text_buffer_place_cursor (buffer, &end);
      search (self, TRUE);
    }
}

static void
replace_all_cb (GtkWidget    *widget,
                MarkerEditor *self)
{
  GtkSourceSearchContext *context = marker_source_get_search_context (self->source_view);
  GtkSourceSearchSettings *settings = gtk_source_search_context_get_settings (context);
  const char *text = gtk_editable_get_text (GTK_EDITABLE (self->search_entry));

  if (*text == '\0')
    return;
  gtk_source_search_settings_set_search_text (settings, text);
  gtk_source_search_context_replace_all (context,
    gtk_editable_get_text (GTK_EDITABLE (self->replace_entry)), -1, NULL);
}

static void
close_search_cb (GtkWidget    *widget,
                 MarkerEditor *self)
{
  gtk_search_bar_set_search_mode (self->search_bar, FALSE);
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
search_next_cb (GtkWidget    *widget,
                MarkerEditor *self)
{
  search (self, TRUE);
}

static void
search_previous_cb (GtkWidget    *widget,
                    MarkerEditor *self)
{
  search (self, FALSE);
}

static void
search_changed_cb (GtkEditable  *editable,
                   MarkerEditor *self)
{
  search (self, TRUE);
}

static GtkWidget *
icon_button (const char *icon,
             const char *tooltip)
{
  GtkWidget *button = gtk_button_new_from_icon_name (icon);
  gtk_widget_add_css_class (button, "flat");
  gtk_widget_set_tooltip_text (button, tooltip);
  return button;
}

static void
format_button_cb (GtkButton    *button,
                  MarkerEditor *self)
{
  const char *format = g_object_get_data (G_OBJECT (button), "marker-format");
  marker_editor_format_selection (self, format);
}

static GtkWidget *
format_button (MarkerEditor *self,
               const char   *icon,
               const char   *tooltip,
               const char   *format)
{
  GtkWidget *button = icon_button (icon, tooltip);
  g_object_set_data_full (G_OBJECT (button), "marker-format", g_strdup (format), g_free);
  g_signal_connect (button, "clicked", G_CALLBACK (format_button_cb), self);
  return button;
}

static void
snippet_button_cb (GtkButton    *button,
                   MarkerEditor *self)
{
  const char *snippet = g_object_get_data (G_OBJECT (button), "marker-snippet");
  int cursor_back = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (button), "marker-cursor-back"));
  marker_editor_insert_snippet (self, snippet, cursor_back);
  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER)));
}

static void
append_snippet_button (GtkBox       *box,
                       MarkerEditor *self,
                       const char   *label,
                       const char   *snippet,
                       int           cursor_back)
{
  GtkWidget *button = gtk_button_new_with_label (label);
  gtk_widget_set_halign (button, GTK_ALIGN_FILL);
  g_object_set_data_full (G_OBJECT (button), "marker-snippet", g_strdup (snippet), g_free);
  g_object_set_data (G_OBJECT (button), "marker-cursor-back", GINT_TO_POINTER (cursor_back));
  g_signal_connect (button, "clicked", G_CALLBACK (snippet_button_cb), self);
  gtk_box_append (box, button);
}

static void
sync_heading_chooser (MarkerSourceView *source, MarkerEditor *self)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self));
  GtkTextIter cursor;
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
  const MarkerHeading *heading = marker_markdown_structure_heading_at_line (
    marker_source_view_get_structure (source), gtk_text_iter_get_line (&cursor));
  self->heading_sync = TRUE;
  gtk_drop_down_set_selected (self->heading_chooser, heading != NULL ? heading->level : 0);
  self->heading_sync = FALSE;
}

static void
heading_changed_cb (GtkDropDown *dropdown, GParamSpec *pspec, MarkerEditor *self)
{
  if (self->heading_sync)
    return;
  guint selected = gtk_drop_down_get_selected (dropdown);
  if (selected > 6)
    return;
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self));
  GtkTextIter cursor;
  gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
  marker_source_view_set_heading_level (self->source_view, gtk_text_iter_get_line (&cursor), selected);
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static void
insert_format_cb (GtkButton *button, MarkerEditor *self)
{
  format_button_cb (button, self);
  gtk_popover_popdown (GTK_POPOVER (gtk_widget_get_ancestor (GTK_WIDGET (button), GTK_TYPE_POPOVER)));
}

static void
append_insert_format (GtkBox *box, MarkerEditor *self, const char *label, const char *format)
{
  GtkWidget *button = gtk_button_new_with_label (label);
  gtk_widget_add_css_class (button, "flat");
  g_object_set_data_full (G_OBJECT (button), "marker-format", g_strdup (format), g_free);
  g_signal_connect (button, "clicked", G_CALLBACK (insert_format_cb), self);
  gtk_box_append (box, button);
}

static void
toggle_page_render_cb (GtkButton    *button,
                       MarkerEditor *self)
{
  self->render_inline = !self->render_inline;
  sync_inline_mode (self);
  marker_editor_refresh_preview (self);
}

static void
source_requested_cb (MarkerRichView *rich, MarkerEditor *self)
{
  if (self->render_inline) toggle_page_render_cb (self->render_button, self);
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

static GtkWidget *
create_format_bar (MarkerEditor *self)
{
  GtkWidget *bar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
  GtkWidget *heading = gtk_drop_down_new_from_strings ((const char *[]) {
    "Paragraph", "Heading 1", "Heading 2", "Heading 3", "Heading 4",
    "Heading 5", "Heading 6", NULL
  });
  GtkWidget *insert_menu = gtk_menu_button_new ();
  GtkWidget *insert_popover = gtk_popover_new ();
  GtkWidget *insert_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *preview_button;
  GtkWidget *spacer = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  gtk_widget_add_css_class (bar, "marker-format-bar");
  gtk_widget_set_hexpand (bar, TRUE);
  self->heading_chooser = GTK_DROP_DOWN (heading);
  gtk_drop_down_set_selected (GTK_DROP_DOWN (heading), 0);
  gtk_widget_set_tooltip_text (heading, "Paragraph style");
  g_signal_connect (heading, "notify::selected", G_CALLBACK (heading_changed_cb), self);
  gtk_box_append (GTK_BOX (bar), heading);
  gtk_box_append (GTK_BOX (bar), format_button (self, "format-text-bold-symbolic", "Bold", "**"));
  gtk_box_append (GTK_BOX (bar), format_button (self, "format-text-italic-symbolic", "Italic", "*"));
  gtk_box_append (GTK_BOX (bar), format_button (self, "format-text-strikethrough-symbolic", "Strikethrough", "~~"));
  gtk_box_append (GTK_BOX (bar), format_button (self, "icon-code-symbolic", "Inline code", "`"));

  gtk_menu_button_set_icon_name (GTK_MENU_BUTTON (insert_menu), "list-add-symbolic");
  gtk_widget_set_tooltip_text (insert_menu, _("Insert"));
  append_insert_format (GTK_BOX (insert_box), self, _("Link"), "link");
  append_insert_format (GTK_BOX (insert_box), self, _("Image"), "image");
  append_insert_format (GTK_BOX (insert_box), self, _("Bulleted list"), "list");
  append_insert_format (GTK_BOX (insert_box), self, _("Blockquote"), "quote");
  gtk_widget_set_margin_top (insert_box, 8);
  gtk_widget_set_margin_bottom (insert_box, 8);
  gtk_widget_set_margin_start (insert_box, 8);
  gtk_widget_set_margin_end (insert_box, 8);
  append_snippet_button (GTK_BOX (insert_box), self, "Display equation", "\n$$\n\n$$\n", 4);
  append_snippet_button (GTK_BOX (insert_box), self, "Mermaid diagram", "\n```mermaid\ngraph TD\n  A --> B\n```\n", 4);
  append_snippet_button (GTK_BOX (insert_box), self, "gnuplot chart", "\n```gnuplot\nplot sin(x)\n```\n", 4);
  append_snippet_button (GTK_BOX (insert_box), self, "Code block", "\n```\n\n```\n", 5);
  append_snippet_button (GTK_BOX (insert_box), self, "Horizontal rule", "\n---\n", 0);
  gtk_popover_set_child (GTK_POPOVER (insert_popover), insert_box);
  gtk_menu_button_set_popover (GTK_MENU_BUTTON (insert_menu), insert_popover);
  gtk_widget_add_css_class (insert_menu, "flat");
  gtk_box_append (GTK_BOX (bar), insert_menu);

  gtk_widget_set_hexpand (spacer, TRUE);
  gtk_box_append (GTK_BOX (bar), spacer);
  preview_button = gtk_button_new_with_label (_("Source"));
  gtk_widget_add_css_class (preview_button, "flat");
  self->render_button = GTK_BUTTON (preview_button);
  gtk_widget_add_css_class (preview_button, "marker-preview-action");
  g_signal_connect (preview_button, "clicked", G_CALLBACK (toggle_page_render_cb), self);
  gtk_box_append (GTK_BOX (bar), preview_button);
  return bar;
}

static GtkWidget *
create_search_bar (MarkerEditor *self)
{
  GtkWidget *bar = gtk_search_bar_new ();
  GtkWidget *rows = gtk_box_new (GTK_ORIENTATION_VERTICAL, 4);
  GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  GtkWidget *previous = icon_button ("go-up-symbolic", "Previous match");
  GtkWidget *next = icon_button ("go-down-symbolic", "Next match");
  GtkWidget *close = icon_button ("window-close-symbolic", "Close search");

  self->search_entry = GTK_SEARCH_ENTRY (gtk_search_entry_new ());
  gtk_widget_set_hexpand (GTK_WIDGET (self->search_entry), TRUE);
  gtk_box_append (GTK_BOX (box), GTK_WIDGET (self->search_entry));
  gtk_box_append (GTK_BOX (box), previous);
  gtk_box_append (GTK_BOX (box), next);
  gtk_box_append (GTK_BOX (box), close);
  gtk_box_append (GTK_BOX (rows), box);
  self->replace_row = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 4);
  self->replace_entry = GTK_ENTRY (gtk_entry_new ());
  gtk_entry_set_placeholder_text (self->replace_entry, _("Replace with…"));
  gtk_widget_set_hexpand (GTK_WIDGET (self->replace_entry), TRUE);
  GtkWidget *replace = gtk_button_new_with_label (_("Replace"));
  GtkWidget *replace_all = gtk_button_new_with_label (_("Replace All"));
  gtk_box_append (GTK_BOX (self->replace_row), GTK_WIDGET (self->replace_entry));
  gtk_box_append (GTK_BOX (self->replace_row), replace);
  gtk_box_append (GTK_BOX (self->replace_row), replace_all);
  gtk_widget_set_visible (self->replace_row, FALSE);
  gtk_box_append (GTK_BOX (rows), self->replace_row);
  gtk_search_bar_set_child (GTK_SEARCH_BAR (bar), rows);
  gtk_search_bar_connect_entry (GTK_SEARCH_BAR (bar), GTK_EDITABLE (self->search_entry));
  g_signal_connect (self->search_entry, "search-changed", G_CALLBACK (search_changed_cb), self);
  g_signal_connect (self->search_entry, "next-match", G_CALLBACK (search_next_cb), self);
  g_signal_connect (self->search_entry, "previous-match", G_CALLBACK (search_previous_cb), self);
  g_signal_connect (previous, "clicked", G_CALLBACK (search_previous_cb), self);
  g_signal_connect (next, "clicked", G_CALLBACK (search_next_cb), self);
  g_signal_connect (replace, "clicked", G_CALLBACK (replace_cb), self);
  g_signal_connect (replace_all, "clicked", G_CALLBACK (replace_all_cb), self);
  g_signal_connect (self->replace_entry, "activate", G_CALLBACK (replace_cb), self);
  g_signal_connect (close, "clicked", G_CALLBACK (close_search_cb), self);
  g_signal_connect (self->search_entry, "stop-search", G_CALLBACK (close_search_cb), self);
  self->search_bar = GTK_SEARCH_BAR (bar);
  return bar;
}

static void
preview_window_closed_cb (GtkWindow    *window,
                          MarkerEditor *self)
{
  self->preview_window = NULL;
  if (self->view_mode == DUAL_WINDOW_MODE)
    {
      self->view_mode = EDITOR_ONLY_MODE;
      marker_editor_update_layout (self);
    }
}

static void
marker_editor_update_layout (MarkerEditor *self)
{
  detach_widget (self->source_scroller);
  detach_widget (GTK_WIDGET (self->preview));
  if (self->paned != NULL)
    {
      detach_widget (GTK_WIDGET (self->paned));
      self->paned = NULL;
    }

  if (self->preview_window != NULL && self->view_mode != DUAL_WINDOW_MODE)
    {
      adw_window_set_content (ADW_WINDOW (self->preview_window), NULL);
      gtk_window_destroy (GTK_WINDOW (self->preview_window));
      self->preview_window = NULL;
    }

  sync_inline_mode (self);
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->source_view),
    self->view_mode == FORMATTED_MODE || marker_prefs_get_wrap_text () ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);
  gtk_widget_set_visible (self->format_bar,
                          self->view_mode == FORMATTED_MODE && !self->focus_mode);

  switch (self->view_mode)
    {
    case PREVIEW_ONLY_MODE:
      gtk_box_append (GTK_BOX (self->content), GTK_WIDGET (self->preview));
      break;

    case DUAL_PANE_MODE:
      self->paned = GTK_PANED (gtk_paned_new (GTK_ORIENTATION_HORIZONTAL));
      gtk_paned_set_start_child (self->paned, self->source_scroller);
      gtk_paned_set_end_child (self->paned, GTK_WIDGET (self->preview));
      gtk_paned_set_resize_start_child (self->paned, TRUE);
      gtk_paned_set_resize_end_child (self->paned, TRUE);
      gtk_paned_set_shrink_start_child (self->paned, FALSE);
      gtk_paned_set_shrink_end_child (self->paned, FALSE);
      gtk_paned_set_position (self->paned, marker_prefs_get_editor_pane_width ());
      gtk_box_append (GTK_BOX (self->content), GTK_WIDGET (self->paned));
      break;

    case DUAL_WINDOW_MODE:
      gtk_box_append (GTK_BOX (self->content), self->source_scroller);
      self->preview_window = adw_window_new ();
      gtk_window_set_title (GTK_WINDOW (self->preview_window), "Marker Preview");
      gtk_window_set_default_size (GTK_WINDOW (self->preview_window), 720, 720);
      adw_window_set_content (ADW_WINDOW (self->preview_window),
                              GTK_WIDGET (self->preview));
      g_signal_connect (self->preview_window, "destroy", G_CALLBACK (preview_window_closed_cb), self);
      gtk_window_present (GTK_WINDOW (self->preview_window));
      break;

    case FORMATTED_MODE:
    case EDITOR_ONLY_MODE:
    default:
      gtk_box_append (GTK_BOX (self->content), self->source_scroller);
      break;
    }

  gtk_widget_set_hexpand (gtk_widget_get_last_child (self->content), TRUE);
  gtk_widget_set_vexpand (gtk_widget_get_last_child (self->content), TRUE);
}

static void
marker_editor_dispose (GObject *object)
{
  MarkerEditor *self = MARKER_EDITOR (object);

  g_clear_handle_id (&self->refresh_source, g_source_remove);
  if (self->preview_window != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->preview_window,
                                            preview_window_closed_cb, self);
      adw_window_set_content (ADW_WINDOW (self->preview_window), NULL);
      gtk_window_destroy (GTK_WINDOW (self->preview_window));
      self->preview_window = NULL;
    }
  g_clear_object (&self->monitor);
  g_clear_object (&self->file);
  if (self->project != NULL)
    g_signal_handlers_disconnect_by_data (self->project, self);
  g_clear_object (&self->project);
  if (self->rich_view != NULL)
    g_object_run_dispose (G_OBJECT (self->rich_view));
  g_clear_object (&self->rich_view);
  g_clear_object (&self->preview);
  g_clear_object (&self->source_scroller);
  G_OBJECT_CLASS (marker_editor_parent_class)->dispose (object);
}

static void
marker_editor_class_init (MarkerEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  object_class->dispose = marker_editor_dispose;

  editor_signals[TITLE_CHANGED] =
    g_signal_new ("title-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  editor_signals[SUBTITLE_CHANGED] =
    g_signal_new ("subtitle-changed", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

static void
marker_editor_init (MarkerEditor *self)
{
  GtkTextBuffer *buffer;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (self), GTK_ORIENTATION_VERTICAL);
  self->view_mode = marker_prefs_get_default_view_mode ();
  self->source_view = marker_source_view_new ();
  self->preview = g_object_ref_sink (marker_preview_new ());
  self->source_scroller = g_object_ref_sink (gtk_scrolled_window_new ());
  self->content = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  self->render_inline = TRUE;
  self->format_bar = create_format_bar (self);

  gtk_widget_add_css_class (self->content, "marker-editor-surface");
  gtk_widget_set_hexpand (GTK_WIDGET (self->source_view), TRUE);
  gtk_widget_set_vexpand (GTK_WIDGET (self->source_view), TRUE);
  gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (self->source_scroller),
                                 GTK_WIDGET (self->source_view));
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (self->source_scroller),
                                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  self->rich_view = marker_rich_view_new (self->source_view);
  g_signal_connect_object (self->rich_view, "source-requested", G_CALLBACK (source_requested_cb), self, 0);
  gtk_box_append (GTK_BOX (self), create_search_bar (self));
  gtk_box_append (GTK_BOX (self), self->content);
  gtk_box_append (GTK_BOX (self), self->format_bar);
  gtk_widget_set_vexpand (self->content, TRUE);

  buffer = gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view));
  g_signal_connect_object (buffer, "changed", G_CALLBACK (buffer_changed_cb), self, 0);
  g_signal_connect_object (buffer, "modified-changed", G_CALLBACK (buffer_modified_cb), self, 0);
  g_signal_connect_swapped (self->preview, "gnuplot-data-changed",
                            G_CALLBACK (queue_preview_refresh), self);
  g_signal_connect_object (self->source_view, "cursor-changed", G_CALLBACK (sync_heading_chooser), self, 0);
  marker_editor_update_layout (self);
  marker_editor_apply_prefs (self);
}

MarkerEditor *
marker_editor_new (void)
{
  return g_object_new (MARKER_TYPE_EDITOR, NULL);
}

MarkerEditor *
marker_editor_new_from_file (GFile *file)
{
  MarkerEditor *self = marker_editor_new ();
  marker_editor_open_file (self, file);
  return self;
}

static void
refresh_preview (MarkerEditor *self, gboolean force_preview)
{
  g_clear_handle_id (&self->refresh_source, g_source_remove);
  g_autofree char *markdown = marker_source_view_get_text (self->source_view);
  g_autofree char *theme = marker_prefs_get_css_theme ();
  g_autofree char *stylesheet = NULL;
  g_autofree char *path = self->file != NULL ? g_file_get_path (self->file) : NULL;

  if (marker_prefs_get_use_css_theme () && theme != NULL && *theme != '\0')
    stylesheet = g_path_is_absolute (theme) ? g_strdup (theme) : g_build_filename (STYLES_DIR, theme, NULL);
  g_autofree char *notebook = self->project != NULL ? g_file_get_path (marker_project_get_root (self->project)) : NULL;
  if (force_preview || self->view_mode != FORMATTED_MODE)
    marker_preview_render_markdown (self->preview, markdown, stylesheet, path, notebook,
                                    marker_source_view_get_cursor_position (self->source_view));
  else
    marker_preview_pause (self->preview);
  if (!force_preview) marker_rich_view_refresh (self->rich_view, stylesheet, path, notebook);
}

void
marker_editor_refresh_preview (MarkerEditor *self)
{
  refresh_preview (self, FALSE);
}

void
marker_editor_print (MarkerEditor *self, GtkWindow *parent)
{
  g_autoptr (MarkerEditor) hold = g_object_ref (self);
  refresh_preview (self, TRUE);
  marker_preview_run_print_dialog (self->preview, parent);
  if (self->view_mode == FORMATTED_MODE) marker_preview_pause (self->preview);
}

MarkerViewMode
marker_editor_get_view_mode (MarkerEditor *self)
{
  g_return_val_if_fail (MARKER_IS_EDITOR (self), EDITOR_ONLY_MODE);
  return self->view_mode;
}

void
marker_editor_set_view_mode (MarkerEditor   *self,
                             MarkerViewMode  view_mode)
{
  g_return_if_fail (MARKER_IS_EDITOR (self));
  if (view_mode < EDITOR_ONLY_MODE || view_mode > FORMATTED_MODE)
    return;
  g_autofree char *filename = self->file != NULL ? g_file_get_basename (self->file) : NULL;
  if (filename != NULL && g_str_has_suffix (filename, ".css"))
    view_mode = EDITOR_ONLY_MODE;
  if (self->view_mode == view_mode && gtk_widget_get_first_child (self->content) != NULL)
    return;
  self->view_mode = view_mode;
  marker_editor_update_layout (self);
  marker_editor_refresh_preview (self);
}


static void
configure_file_language (MarkerEditor *self)
{
  g_autofree char *filename = g_file_get_basename (self->file);
  gboolean css = g_str_has_suffix (filename, ".css");
  marker_source_view_set_language (self->source_view, css ? "css" : "markdown");
  if (css)
    marker_editor_set_view_mode (self, EDITOR_ONLY_MODE);
}

void
marker_editor_open_file (MarkerEditor *self,
                         GFile        *file)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *contents = NULL;
  gsize length = 0;

  g_return_if_fail (MARKER_IS_EDITOR (self));
  g_return_if_fail (G_IS_FILE (file));

  if (!g_file_load_contents (file, NULL, &contents, &length, NULL, &error))
    {
      g_warning ("Unable to open file: %s", error->message);
      return;
    }

  self->loading = TRUE;
  g_set_object (&self->file, file);
  configure_file_language (self);
  marker_source_view_set_text (self->source_view, contents, length);
  self->loading = FALSE;
  configure_file_monitor (self);
  marker_editor_refresh_preview (self);
  g_signal_emit (self, editor_signals[TITLE_CHANGED], 0);
  g_signal_emit (self, editor_signals[SUBTITLE_CHANGED], 0);
}

void
marker_editor_reload_file (MarkerEditor *self)
{
  if (self->file != NULL)
    marker_editor_open_file (self, self->file);
}

gboolean
marker_editor_save_file (MarkerEditor *self)
{
  g_autoptr (GError) error = NULL;
  g_autofree char *text = NULL;

  if (self->file == NULL)
    return FALSE;
  text = marker_source_view_get_text (self->source_view);
  if (!g_file_replace_contents (self->file, text, strlen (text), NULL, FALSE,
                                G_FILE_CREATE_NONE, NULL, NULL, &error))
    {
      g_warning ("Unable to save file: %s", error->message);
      return FALSE;
    }
  marker_source_view_set_modified (self->source_view, FALSE);
  configure_file_monitor (self);
  return TRUE;
}

gboolean
marker_editor_save_file_as (MarkerEditor *self,
                            GFile        *file)
{
  g_autoptr (GFile) previous_file = self->file != NULL
                                      ? g_object_ref (self->file) : NULL;

  g_set_object (&self->file, file);
  if (!marker_editor_save_file (self))
    {
      g_set_object (&self->file, previous_file);
      return FALSE;
    }
  configure_file_language (self);
  marker_editor_refresh_preview (self);
  g_signal_emit (self, editor_signals[TITLE_CHANGED], 0);
  g_signal_emit (self, editor_signals[SUBTITLE_CHANGED], 0);
  return TRUE;
}

GFile *
marker_editor_get_file (MarkerEditor *self)
{
  return self->file;
}

MarkerProject *
marker_editor_get_project (MarkerEditor *self)
{
  return self->project;
}

static void
notebook_styles_changed (MarkerProject *project, MarkerEditor *self)
{
  g_autofree char *name = self->file != NULL ? g_file_get_basename (self->file) : NULL;
  if (g_strcmp0 (name, ".marker.css") == 0 && !marker_editor_has_unsaved_changes (self) &&
      g_file_query_exists (self->file, NULL))
    marker_editor_reload_file (self);
  else
    queue_preview_refresh (self);
}

void
marker_editor_set_project (MarkerEditor *self, MarkerProject *project)
{
  if (self->project == project)
    return;
  if (self->project != NULL)
    g_signal_handlers_disconnect_by_data (self->project, self);
  g_set_object (&self->project, project);
  if (project != NULL)
    {
      g_signal_connect_object (project, "styles-changed", G_CALLBACK (notebook_styles_changed), self, 0);
      g_signal_connect_object (project, "notify::root", G_CALLBACK (queue_preview_refresh), self, G_CONNECT_SWAPPED);
    }
  queue_preview_refresh (self);
}

gboolean
marker_editor_has_unsaved_changes (MarkerEditor *self)
{
  return marker_source_view_get_modified (self->source_view);
}

gchar *
marker_editor_get_raw_title (MarkerEditor *self)
{
  if (self->file == NULL)
    return g_strdup ("Untitled");
  return g_file_get_basename (self->file);
}

gchar *
marker_editor_get_title (MarkerEditor *self)
{
  g_autofree char *title = marker_editor_get_page_title (self);
  return marker_editor_has_unsaved_changes (self)
           ? g_strdup_printf ("%s •", title)
           : g_steal_pointer (&title);
}

gchar *
marker_editor_get_subtitle (MarkerEditor *self)
{
  return self->file != NULL ? g_file_get_parse_name (self->file) : g_strdup ("Unsaved document");
}

MarkerPreview *
marker_editor_get_preview (MarkerEditor *self)
{
  return self->preview;
}

MarkerSourceView *
marker_editor_get_source_view (MarkerEditor *self)
{
  return self->source_view;
}

GtkSourceBuffer *
marker_editor_get_buffer (MarkerEditor *self)
{
  return GTK_SOURCE_BUFFER (gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source_view)));
}

void
marker_editor_apply_prefs (MarkerEditor *self)
{
  g_autofree char *theme = marker_prefs_get_syntax_theme ();
  g_autofree char *language = marker_prefs_get_spell_check_language ();

  marker_source_view_apply_font (self->source_view);
  marker_source_view_set_syntax_theme (self->source_view, theme);
  marker_source_view_set_spell_check (self->source_view, marker_prefs_get_spell_check ());
  marker_source_view_set_spell_check_lang (self->source_view, language);
  marker_source_view_set_writing_width (self->source_view,
                                       g_settings_get_uint (prefs.editor_settings, "writing-width"));
  gtk_source_view_set_show_line_numbers (GTK_SOURCE_VIEW (self->source_view),
                                         marker_prefs_get_show_line_numbers ());
  gtk_source_view_set_highlight_current_line (GTK_SOURCE_VIEW (self->source_view),
                                              marker_prefs_get_highlight_current_line ());
  gtk_source_view_set_show_right_margin (GTK_SOURCE_VIEW (self->source_view),
                                         self->view_mode != FORMATTED_MODE && marker_prefs_get_show_right_margin ());
  gtk_source_view_set_right_margin_position (GTK_SOURCE_VIEW (self->source_view),
                                             marker_prefs_get_right_margin_position ());
  gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (self->source_view),
                               marker_prefs_get_wrap_text () || self->view_mode == FORMATTED_MODE
                                 ? GTK_WRAP_WORD_CHAR : GTK_WRAP_NONE);
  marker_source_view_reapply_formatted (self->source_view);
  marker_editor_refresh_preview (self);
}

void
marker_editor_closing (MarkerEditor *self)
{
  g_clear_handle_id (&self->refresh_source, g_source_remove);
  if (self->paned != NULL)
    marker_prefs_set_editor_pane_width (gtk_paned_get_position (self->paned));
  if (self->preview_window != NULL)
    {
      g_signal_handlers_disconnect_by_func (self->preview_window, preview_window_closed_cb, self);
      gtk_window_destroy (GTK_WINDOW (self->preview_window));
      self->preview_window = NULL;
    }
}


void
marker_editor_toggle_search_bar (MarkerEditor *self)
{
  gboolean enabled = !gtk_search_bar_get_search_mode (self->search_bar) ||
                     gtk_widget_get_visible (self->replace_row);
  gtk_widget_set_visible (self->replace_row, FALSE);
  gtk_search_bar_set_search_mode (self->search_bar, enabled);
  if (enabled)
    gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}

void
marker_editor_show_replace_bar (MarkerEditor *self)
{
  if (self->view_mode == PREVIEW_ONLY_MODE)
    marker_editor_set_view_mode (self, EDITOR_ONLY_MODE);
  gtk_widget_set_visible (self->replace_row, TRUE);
  gtk_search_bar_set_search_mode (self->search_bar, TRUE);
  gtk_widget_grab_focus (GTK_WIDGET (self->search_entry));
}


void
marker_editor_set_focus_mode (MarkerEditor *self,
                              gboolean      enabled)
{
  self->focus_mode = enabled;
  gtk_widget_set_visible (self->format_bar,
                          self->view_mode == FORMATTED_MODE && !enabled);
}

void
marker_editor_scroll_preview_to_cursor (MarkerEditor *self)
{
  marker_editor_refresh_preview (self);
  marker_preview_scroll_to_cursor (self->preview);
}

static void
prefix_current_lines (GtkTextBuffer *buffer,
                      const char    *prefix)
{
  GtkTextIter start;
  GtkTextIter end;
  gboolean has_selection;
  gint first_line;
  gint last_line;

  has_selection = gtk_text_buffer_get_selection_bounds (buffer, &start, &end);
  if (!has_selection)
    gtk_text_buffer_get_iter_at_mark (buffer, &start,
                                      gtk_text_buffer_get_insert (buffer));
  first_line = gtk_text_iter_get_line (&start);
  last_line = has_selection ? gtk_text_iter_get_line (&end) : first_line;
  if (has_selection && gtk_text_iter_get_line_offset (&end) == 0 &&
      last_line > first_line)
    last_line--;

  gtk_text_buffer_begin_user_action (buffer);
  for (gint line = first_line; line <= last_line; line++)
    {
      gtk_text_buffer_get_iter_at_line (buffer, &start, line);
      gtk_text_buffer_insert (buffer, &start, prefix, -1);
    }
  gtk_text_buffer_end_user_action (buffer);
}

void
marker_editor_format_selection (MarkerEditor *self,
                                const char   *format)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self));

  if (g_str_equal (format, "link"))
    marker_source_view_insert_link (self->source_view);
  else if (g_str_equal (format, "image"))
    marker_editor_insert_snippet (self, "![]()", 3);
  else if (g_str_equal (format, "list"))
    prefix_current_lines (buffer, "- ");
  else if (g_str_equal (format, "quote"))
    prefix_current_lines (buffer, "> ");
  else
    marker_source_view_surround_selection_with (self->source_view, format);
}

void
marker_editor_insert_snippet (MarkerEditor *self,
                              const char   *snippet,
                              gint          cursor_back)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self));
  GtkTextIter cursor;

  gtk_text_buffer_get_iter_at_mark (buffer, &cursor, gtk_text_buffer_get_insert (buffer));
  gtk_text_buffer_begin_user_action (buffer);
  gtk_text_buffer_insert (buffer, &cursor, snippet, -1);
  if (cursor_back > 0)
    {
      gtk_text_iter_backward_chars (&cursor, cursor_back);
      gtk_text_buffer_place_cursor (buffer, &cursor);
    }
  gtk_text_buffer_end_user_action (buffer);
  gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
}

char *
marker_editor_get_page_title (MarkerEditor *self)
{
  g_autofree char *filename = marker_editor_get_raw_title (self);
  if (self->file != NULL && g_str_has_suffix (filename, ".css"))
    return g_steal_pointer (&filename);
  const MarkerMarkdownStructure *structure = marker_source_view_get_structure (self->source_view);
  for (guint i = 0; i < structure->headings->len; i++)
    {
      const MarkerHeading *heading = g_ptr_array_index (structure->headings, i);
      if (*heading->title != '\0')
        return g_strdup (heading->title);
    }
  if (g_str_has_suffix (filename, ".md"))
    filename[strlen (filename) - 3] = '\0';
  return g_steal_pointer (&filename);
}

void
marker_editor_jump_to_heading (MarkerEditor *self, guint offset)
{
  GtkTextBuffer *buffer = GTK_TEXT_BUFFER (marker_editor_get_buffer (self));
  GtkTextIter iter;
  gtk_text_buffer_get_iter_at_offset (buffer, &iter, offset);
  gtk_text_buffer_place_cursor (buffer, &iter);
  gtk_text_view_scroll_to_iter (GTK_TEXT_VIEW (self->source_view), &iter, .15, TRUE, 0, .2);
  if (self->view_mode != PREVIEW_ONLY_MODE)
    gtk_widget_grab_focus (GTK_WIDGET (self->source_view));
  marker_editor_scroll_preview_to_cursor (self);
}
