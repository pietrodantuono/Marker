/* Copyright (C) 2026 Marker contributors; SPDX-License-Identifier: GPL-3.0-or-later */
#include "marker-rich-view.h"
#include "marker-preview.h"
#include <string.h>
#include <glib/gi18n.h>

/* GtkTextView has no public per-overlay removal API. Own a single zero-measure
 * layer; manage its children normally and leave the empty layer with the view
 * until view disposal. Its coordinates scroll with the text view. */
typedef struct { GtkWidget parent_instance; } RichLayer;
typedef struct { GtkWidgetClass parent_class; } RichLayerClass;
G_DEFINE_TYPE (RichLayer, rich_layer, GTK_TYPE_WIDGET)

static void
rich_layer_measure (GtkWidget *widget, GtkOrientation orientation, int for_size,
                    int *minimum, int *natural, int *minimum_baseline, int *natural_baseline)
{
  *minimum = *natural = 0;
  *minimum_baseline = *natural_baseline = -1;
}

static void
rich_layer_size_allocate (GtkWidget *widget, int width, int height, int baseline)
{
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    {
      if (!gtk_widget_get_visible (child)) continue;
      GtkRequisition size;
      gtk_widget_get_preferred_size (child, &size, NULL);
      GtkAllocation allocation = { GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "rich-x")),
        GPOINTER_TO_INT (g_object_get_data (G_OBJECT (child), "rich-y")), size.width, size.height };
      gtk_widget_size_allocate (child, &allocation, -1);
    }
}

static void
rich_layer_snapshot (GtkWidget *widget, GtkSnapshot *snapshot)
{
  for (GtkWidget *child = gtk_widget_get_first_child (widget); child != NULL;
       child = gtk_widget_get_next_sibling (child))
    if (gtk_widget_get_visible (child)) gtk_widget_snapshot_child (widget, child, snapshot);
}

static void
rich_layer_dispose (GObject *object)
{
  GtkWidget *child;
  while ((child = gtk_widget_get_first_child (GTK_WIDGET (object))) != NULL)
    gtk_widget_unparent (child);
  G_OBJECT_CLASS (rich_layer_parent_class)->dispose (object);
}

static void
rich_layer_class_init (RichLayerClass *klass)
{
  GTK_WIDGET_CLASS (klass)->measure = rich_layer_measure;
  GTK_WIDGET_CLASS (klass)->size_allocate = rich_layer_size_allocate;
  GTK_WIDGET_CLASS (klass)->snapshot = rich_layer_snapshot;
  G_OBJECT_CLASS (klass)->dispose = rich_layer_dispose;
}

static void rich_layer_init (RichLayer *self) { }

/* SourceView owns the text/undo/selection; this object owns only presentation.
 * A single offscreen WebKit renderer supplies all regions. Never insert anchors
 * or use invisible tags in the authoritative GtkSourceBuffer. */
typedef struct
{
  MarkerRichView *owner;
  GtkTextMark *start, *end;
  GtkTextTag *spacing;
  GtkWidget *box, *picture, *message;
  gboolean source;
} RichItem;

struct _MarkerRichView
{
  GObject parent_instance;
  MarkerSourceView *source;
  MarkerPreview *renderer;
  GtkWidget *layer;
  GPtrArray *items;
  GArray *rects;
  GtkTextTag *hidden;
  GCancellable *cancellable;
  char *markdown, *token, *theme, *path, *notebook;
  guint generation, layout_idle;
  guint render_timeout;
  int last_cursor, last_selection;
  int snapshot_width;
  int width;
  gboolean enabled, applying, dirty, disposed;
};
G_DEFINE_TYPE (MarkerRichView, marker_rich_view, G_TYPE_OBJECT)

typedef struct { MarkerRichView *self; guint generation; } RenderRequest;
static void queue_layout (MarkerRichView *self);
static void rebuild (MarkerRichView *self);
static void ensure_renderer (MarkerRichView *self);

static GtkTextBuffer *
source_buffer (MarkerRichView *self)
{
  return gtk_text_view_get_buffer (GTK_TEXT_VIEW (self->source));
}

static void
item_bounds (RichItem *item, GtkTextIter *start, GtkTextIter *end)
{
  GtkTextBuffer *buffer = source_buffer (item->owner);
  gtk_text_buffer_get_iter_at_mark (buffer, start, item->start);
  gtk_text_buffer_get_iter_at_mark (buffer, end, item->end);
}

static void
item_free (gpointer data)
{
  RichItem *item = data;
  GtkTextBuffer *buffer = source_buffer (item->owner);
  gtk_widget_unparent (item->box);
  gtk_text_buffer_delete_mark (buffer, item->start);
  gtk_text_buffer_delete_mark (buffer, item->end);
  gtk_text_tag_table_remove (gtk_text_buffer_get_tag_table (buffer), item->spacing);
  g_free (item);
}

static void
clear_items (MarkerRichView *self)
{
  GtkTextIter start, end;
  gtk_text_buffer_get_bounds (source_buffer (self), &start, &end);
  if (self->hidden != NULL)
    gtk_text_buffer_remove_tag (source_buffer (self), self->hidden, &start, &end);
  g_ptr_array_set_size (self->items, 0);
}

static void
apply_item (RichItem *item)
{
  MarkerRichView *self = item->owner;
  GtkTextBuffer *buffer = source_buffer (self);
  GtkTextIter start, end, first_end;
  item_bounds (item, &start, &end);
  first_end = start;
  gtk_text_iter_forward_char (&first_end);
  gtk_text_buffer_remove_tag (buffer, self->hidden, &start, &end);
  gboolean has_picture = gtk_picture_get_paintable (GTK_PICTURE (item->picture)) != NULL;
  gboolean source = item->source;
  if (source)
    {
      GtkTextTag *delimiters = gtk_text_tag_table_lookup (gtk_text_buffer_get_tag_table (buffer), "marker-delimiter-hidden");
      if (delimiters != NULL) gtk_text_buffer_remove_tag (buffer, delimiters, &start, &end);
    }
  gtk_widget_set_visible (item->picture, !source && has_picture);
  gtk_widget_set_size_request (item->box, self->width, -1);
  GtkRequisition size;
  gtk_widget_get_preferred_size (item->box, &size, NULL);
  int space = size.height;
  g_object_set (item->spacing, "pixels-above-lines", space, NULL);
  gtk_text_buffer_apply_tag (buffer, item->spacing, &start, &first_end);
  if (!source && has_picture)
    gtk_text_buffer_apply_tag (buffer, self->hidden, &start, &end);
  gtk_widget_set_visible (item->box, TRUE);
  queue_layout (self);
}

static void
show_error (MarkerRichView *self, const char *message)
{
  g_clear_handle_id (&self->render_timeout, g_source_remove);
  for (guint i = 0; i < self->items->len; i++)
    {
      RichItem *item = g_ptr_array_index (self->items, i);
      item->source = TRUE;
      gtk_label_set_text (GTK_LABEL (item->message), message);
      gtk_widget_set_visible (item->message, TRUE);
      apply_item (item);
    }
}

static void
show_source (RichItem *item)
{
  MarkerRichView *self = item->owner;
  self->applying = TRUE;
  item->source = TRUE;
  apply_item (item);
  self->applying = FALSE;
}

static void
claim_press (GtkGestureClick *gesture, int count, double x, double y, gpointer data)
{
  /* Keep the ancestor text view from selecting beneath a rendered element. */
  gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
}

static void
picture_clicked (GtkGestureClick *gesture, int count, double x, double y, RichItem *item)
{
  g_signal_emit_by_name (item->owner, "source-requested");
}

static RenderRequest *
request_new (MarkerRichView *self)
{
  RenderRequest *request = g_new0 (RenderRequest, 1);
  request->self = g_object_ref (self);
  request->generation = self->generation;
  return request;
}

static void
request_free (RenderRequest *request)
{
  g_object_unref (request->self);
  g_free (request);
}

static gboolean
request_current (RenderRequest *request)
{
  return !request->self->disposed && request->self->enabled &&
         request->generation == request->self->generation;
}

static void
snapshot_finished (GObject *object, GAsyncResult *result, gpointer data)
{
  RenderRequest *request = data;
  MarkerRichView *self = request->self;
  g_autoptr (GError) error = NULL;
  g_autoptr (GdkTexture) texture = webkit_web_view_get_snapshot_finish (WEBKIT_WEB_VIEW (object), result, &error);
  if (!request_current (request)) { request_free (request); return; }
  if (texture == NULL)
    {
      show_error (self, _("Rendering unavailable. Edit source or Render to retry."));
      request_free (request);
      return;
    }

  int width = gdk_texture_get_width (texture), height = gdk_texture_get_height (texture);
  /* 64 MiB budget checked before requesting the snapshot as well as readback. */
  if ((guint64) width * height > 16 * 1024 * 1024)
    { show_error (self, _("Document too large for inline rendering. Use Preview.")); request_free (request); return; }
  double scale = (double) width / MAX (1, self->snapshot_width);
  g_clear_handle_id (&self->render_timeout, g_source_remove);
  gsize stride = width * 4;
  g_autofree guchar *pixels = g_malloc_n (height, stride);
  gdk_texture_download (texture, pixels, stride);
  for (guint i = 0; i < self->items->len && i < self->rects->len; i++)
    {
      RichItem *item = g_ptr_array_index (self->items, i);
      GdkRectangle rect = g_array_index (self->rects, GdkRectangle, i);
      int display_width = rect.width, display_height = rect.height;
      rect.x *= scale; rect.y *= scale;
      rect.width *= scale; rect.height *= scale;
      rect.x = CLAMP (rect.x, 0, width - 1);
      rect.y = CLAMP (rect.y, 0, height - 1);
      rect.width = MIN (rect.width, width - rect.x);
      rect.height = MIN (rect.height, height - rect.y);
      if (rect.width <= 0 || rect.height <= 0) continue;
      gsize row = rect.width * 4;
      guchar *crop = g_malloc_n (rect.height, row);
      for (int y = 0; y < rect.height; y++)
        memcpy (crop + y * row, pixels + (rect.y + y) * stride + rect.x * 4, row);
      g_autoptr (GBytes) bytes = g_bytes_new_take (crop, row * rect.height);
      g_autoptr (GdkTexture) image = gdk_memory_texture_new (rect.width, rect.height,
        GDK_MEMORY_DEFAULT, bytes, row);
      gtk_picture_set_paintable (GTK_PICTURE (item->picture), GDK_PAINTABLE (image));
      if (display_width > self->width)
        {
          display_height = MAX (1, (gint64) display_height * self->width / display_width);
          display_width = self->width;
        }
      gtk_widget_set_size_request (item->picture, display_width, display_height);
      apply_item (item);
    }
  request_free (request);
}

static void
rects_finished (GObject *object, GAsyncResult *result, gpointer data)
{
  RenderRequest *request = data;
  MarkerRichView *self = request->self;
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value = webkit_web_view_call_async_javascript_function_finish (WEBKIT_WEB_VIEW (object), result, &error);
  if (!request_current (request)) { request_free (request); return; }
  if (value == NULL)
    { show_error (self, _("Rendering unavailable. Edit source or Render to retry.")); request_free (request); return; }
  g_array_set_size (self->rects, 0);
  g_autoptr (JSCValue) size = jsc_value_object_get_property (value, "pixels");
  int scale = gtk_widget_get_scale_factor (GTK_WIDGET (self->source));
  if (jsc_value_to_double (size) * scale * scale > 16 * 1024 * 1024)
    {
      show_error (self, _("Document too large for inline rendering. Use Preview."));
      request_free (request); return;
    }
  g_autoptr (JSCValue) snapshot_width = jsc_value_object_get_property (value, "width");
  self->snapshot_width = jsc_value_to_int32 (snapshot_width);
  g_autoptr (JSCValue) rects = jsc_value_object_get_property (value, "rects");
  for (guint i = 0; i < self->items->len; i++)
    {
      g_autoptr (JSCValue) rect = jsc_value_object_get_property_at_index (rects, i);
      const char *properties[] = { "x", "y", "width", "height" };
      int numbers[4];
      for (guint n = 0; n < 4; n++)
        {
          g_autoptr (JSCValue) number = jsc_value_object_get_property (rect, properties[n]);
          numbers[n] = jsc_value_to_int32 (number);
        }
      GdkRectangle bounds = { numbers[0], numbers[1], numbers[2], numbers[3] };
      g_array_append_val (self->rects, bounds);
      g_autoptr (JSCValue) failure = jsc_value_object_get_property (rect, "error");
      RichItem *item = g_ptr_array_index (self->items, i);
      gtk_widget_set_visible (item->message, FALSE);
      if (jsc_value_is_string (failure))
        {
          g_autofree char *message = jsc_value_to_string (failure);
          const char *description = _("Unable to render this region. Edit source or Render to retry.");
          if (g_str_equal (message, "plot")) description = _("Plot failed. Edit source and Render to retry.");
          else if (g_str_equal (message, "image")) description = _("Image unavailable. Check its path and Render to retry.");
          else if (g_str_equal (message, "equation")) description = _("Equation failed. Edit source and Render to retry.");
          else if (g_str_equal (message, "disabled")) description = _("Renderer disabled or unavailable. Check Preview preferences.");
          gtk_label_set_text (GTK_LABEL (item->message), description);
          gtk_widget_set_visible (item->message, TRUE);
          item->source = TRUE;
          apply_item (item);
        }
    }
  webkit_web_view_get_snapshot (WEBKIT_WEB_VIEW (self->renderer), WEBKIT_SNAPSHOT_REGION_FULL_DOCUMENT,
    WEBKIT_SNAPSHOT_OPTIONS_TRANSPARENT_BACKGROUND, self->cancellable,
    snapshot_finished, request_new (self));
  request_free (request);
}

static void
render_complete (MarkerPreview *preview, MarkerRichView *self)
{
  if (!self->enabled || self->dirty || self->items->len == 0) return;
  g_autoptr (GBytes) bytes = g_resources_lookup_data (
    "/com/github/fabiocolacio/marker/scripts/marker-rich-snapshot.js", 0, NULL);
  if (bytes == NULL)
    { show_error (self, _("Rendering unavailable. Edit source or Render to retry.")); return; }
  g_autofree char *body = g_strndup (g_bytes_get_data (bytes, NULL), g_bytes_get_size (bytes));
  g_autofree char *script = g_strdup_printf ("%s\nreturn await markerRichSnapshot(%u,'%s');",
                                            body, self->items->len, self->token);
  webkit_web_view_call_async_javascript_function (WEBKIT_WEB_VIEW (preview), script, -1,
    NULL, NULL, NULL, self->cancellable, rects_finished, request_new (self));
}

static gboolean
layout_idle (gpointer data)
{
  MarkerRichView *self = data;
  self->layout_idle = 0;
  if (!self->enabled || self->disposed) return G_SOURCE_REMOVE;
  GtkTextView *view = GTK_TEXT_VIEW (self->source);
  GdkRectangle visible;
  gtk_text_view_get_visible_rect (view, &visible);
  int width = MAX (80, visible.width - gtk_text_view_get_left_margin (view) - gtk_text_view_get_right_margin (view));
  if (width != self->width && gtk_widget_get_width (GTK_WIDGET (self->source)) > 0)
    {
      self->width = width;
      if (self->renderer != NULL)
        gtk_widget_set_size_request (GTK_WIDGET (self->renderer), width, 400);
      rebuild (self);
    }
  gboolean moved = FALSE;
  for (guint i = 0; i < self->items->len; i++)
    {
      RichItem *item = g_ptr_array_index (self->items, i);
      GtkTextIter start, end;
      GdkRectangle location;
      item_bounds (item, &start, &end);
      gtk_text_view_get_iter_location (view, &start, &location);
      int space;
      g_object_get (item->spacing, "pixels-above-lines", &space, NULL);
      int x = gtk_text_view_get_left_margin (view), y = location.y - space;
      if (x != GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item->box), "rich-x")) ||
          y != GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item->box), "rich-y")))
        {
          g_object_set_data (G_OBJECT (item->box), "rich-x", GINT_TO_POINTER (x));
          g_object_set_data (G_OBJECT (item->box), "rich-y", GINT_TO_POINTER (y));
          moved = TRUE;
        }
    }
  if (moved) gtk_widget_queue_allocate (self->layer);
  return G_SOURCE_REMOVE;
}

static void
queue_layout (MarkerRichView *self)
{
  if (!self->disposed && self->layout_idle == 0)
    self->layout_idle = g_idle_add (layout_idle, self);
}

static void
scroll_changed (MarkerRichView *self)
{
  /* GTK 4.14 redraws unanchored text overlays on scroll without reallocating
   * them. Refresh the view allocation so its overlay coordinates also move. */
  if (self->enabled && !self->disposed)
    {
      gtk_text_view_move_overlay (GTK_TEXT_VIEW (self->source), self->layer, 0, 0);
      gtk_widget_queue_allocate (GTK_WIDGET (self->source));
    }
}

static void
cursor_changed (MarkerSourceView *source, MarkerRichView *self)
{
  if (!self->enabled || self->applying) return;
  GtkTextBuffer *buffer = source_buffer (self);
  GtkTextIter selection_start, selection_end;
  if (!gtk_text_buffer_get_selection_bounds (buffer, &selection_start, &selection_end))
    {
      gtk_text_buffer_get_iter_at_mark (buffer, &selection_start, gtk_text_buffer_get_insert (buffer));
      selection_end = selection_start;
    }
  int cursor = gtk_text_iter_get_offset (&selection_start);
  int selection = gtk_text_iter_get_offset (&selection_end);
  for (guint i = 0; i < self->items->len; i++)
    {
      RichItem *item = g_ptr_array_index (self->items, i);
      if (item->source) apply_item (item);
    }
  if (cursor == self->last_cursor && selection == self->last_selection)
    { queue_layout (self); return; }
  self->last_cursor = cursor;
  self->last_selection = selection;
  for (guint i = 0; i < self->items->len; i++)
    {
      RichItem *item = g_ptr_array_index (self->items, i);
      GtkTextIter start, end;
      item_bounds (item, &start, &end);
      if (gtk_text_iter_compare (&selection_start, &end) <= 0 &&
          gtk_text_iter_compare (&selection_end, &start) >= 0)
        show_source (item);
    }
  queue_layout (self);
}

static void
invalidate (MarkerRichView *self)
{
  g_clear_handle_id (&self->render_timeout, g_source_remove);
  self->generation++;
  g_cancellable_cancel (self->cancellable);
  g_clear_object (&self->cancellable);
  self->cancellable = g_cancellable_new ();
  self->dirty = TRUE;
}

static void
buffer_changed (GtkTextBuffer *buffer, MarkerRichView *self)
{
  if (!self->enabled) return;
  invalidate (self);
  /* Reveal immediately: old textures must never conceal newly inserted text. */
  for (guint i = 0; i < self->items->len; i++)
    show_source (g_ptr_array_index (self->items, i));
}

static gboolean
render_timed_out (gpointer data)
{
  MarkerRichView *self = data;
  self->render_timeout = 0;
  invalidate (self);
  show_error (self, _("Rendering timed out. Edit source or Render to retry."));
  return G_SOURCE_REMOVE;
}

static void
rebuild (MarkerRichView *self)
{
  if (!self->enabled || self->disposed || self->width == 0) return;
  int cursor = marker_source_view_get_cursor_position (self->source);
  gboolean editing = FALSE;
  for (guint i = 0; i < self->items->len; i++)
    {
      RichItem *item = g_ptr_array_index (self->items, i);
      GtkTextIter start, end;
      item_bounds (item, &start, &end);
      editing |= item->source && cursor >= gtk_text_iter_get_offset (&start) && cursor <= gtk_text_iter_get_offset (&end);
    }
  invalidate (self);
  clear_items (self);
  GtkSourceLanguage *language = gtk_source_buffer_get_language (GTK_SOURCE_BUFFER (source_buffer (self)));
  if (language == NULL || !g_str_equal (gtk_source_language_get_id (language), "markdown")) return;
  g_clear_pointer (&self->markdown, g_free);
  self->markdown = marker_source_view_get_text (self->source);
  g_autoptr (GArray) ranges = marker_markdown_structure_rich_ranges (self->markdown,
    marker_source_view_get_structure (self->source));
  if (ranges->len == 0)
    {
      self->dirty = FALSE;
      if (self->renderer != NULL)
        {
          marker_preview_pause (self->renderer);
          gtk_widget_set_visible (GTK_WIDGET (self->renderer), FALSE);
        }
      return;
    }
  ensure_renderer (self);
  gtk_widget_set_visible (GTK_WIDGET (self->renderer), TRUE);
  GtkTextBuffer *buffer = source_buffer (self);
  if (self->hidden == NULL)
    {
      GdkRGBA clear = { 0, 0, 0, 0 };
      self->hidden = gtk_text_buffer_create_tag (buffer, "marker-rich-hidden", "foreground-rgba", &clear,
        "background-rgba", &clear, "scale", .01, "pixels-above-lines", 0,
        "pixels-below-lines", 0, "pixels-inside-wrap", 0, NULL);
    }
  gtk_text_tag_set_priority (self->hidden, gtk_text_tag_table_get_size (gtk_text_buffer_get_tag_table (buffer)) - 1);
  for (guint i = 0; i < ranges->len; i++)
    {
      const MarkerRichRange *range = &g_array_index (ranges, MarkerRichRange, i);
      RichItem *item = g_new0 (RichItem, 1);
      item->owner = self;
      item->source = editing && cursor >= (int) range->start && cursor <= (int) range->end;
      GtkTextIter start, end;
      gtk_text_buffer_get_iter_at_offset (buffer, &start, range->start);
      gtk_text_buffer_get_iter_at_offset (buffer, &end, range->end);
      item->start = gtk_text_buffer_create_mark (buffer, NULL, &start, TRUE);
      item->end = gtk_text_buffer_create_mark (buffer, NULL, &end, FALSE);
      item->spacing = gtk_text_buffer_create_tag (buffer, NULL, "pixels-above-lines", 0, NULL);
      item->box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
      gtk_widget_add_css_class (item->box, "marker-rich-region");
      item->message = gtk_label_new (NULL);
      gtk_label_set_text (GTK_LABEL (item->message), _("Rendering…"));
      gtk_label_set_wrap (GTK_LABEL (item->message), TRUE);
      gtk_widget_add_css_class (item->message, "dim-label");
      gtk_box_append (GTK_BOX (item->box), item->message);
      item->picture = gtk_picture_new ();
      gtk_widget_set_halign (item->picture, GTK_ALIGN_START);
      gtk_picture_set_can_shrink (GTK_PICTURE (item->picture), TRUE);
      gtk_picture_set_alternative_text (GTK_PICTURE (item->picture), "Rendered Markdown element. Activate Source to edit.");
      gtk_widget_set_cursor_from_name (item->picture, "text");
      gtk_box_append (GTK_BOX (item->box), item->picture);
      GtkGesture *click = gtk_gesture_click_new ();
      gtk_widget_add_controller (item->picture, GTK_EVENT_CONTROLLER (click));
      g_signal_connect (click, "pressed", G_CALLBACK (claim_press), NULL);
      g_signal_connect (click, "released", G_CALLBACK (picture_clicked), item);
      gtk_widget_set_parent (item->box, self->layer);
      g_ptr_array_add (self->items, item);
      apply_item (item);
    }
  self->dirty = FALSE;
  self->render_timeout = g_timeout_add_seconds (30, render_timed_out, self);
  g_autofree char *annotated = marker_markdown_structure_rich_source (self->markdown, ranges, self->token);
  marker_preview_render_markdown (self->renderer, annotated, self->theme, self->path, self->notebook, -1);
}

void
marker_rich_view_refresh (MarkerRichView *self, const char *theme, const char *path, const char *notebook)
{
  g_free (self->theme); self->theme = g_strdup (theme);
  g_free (self->path); self->path = g_strdup (path);
  g_free (self->notebook); self->notebook = g_strdup (notebook);
  rebuild (self);
}

void
marker_rich_view_set_enabled (MarkerRichView *self, gboolean enabled)
{
  if (self->enabled == enabled) return;
  self->enabled = enabled;
  self->last_cursor = self->last_selection = marker_source_view_get_cursor_position (self->source);
  invalidate (self);
  if (enabled) queue_layout (self);
  else
    {
      clear_items (self);
      if (self->renderer != NULL)
        {
          marker_preview_pause (self->renderer);
          gtk_widget_unparent (GTK_WIDGET (self->renderer));
          g_object_run_dispose (G_OBJECT (self->renderer));
          g_clear_object (&self->renderer);
        }
    }
}

static void
data_changed (MarkerPreview *preview, MarkerRichView *self)
{
  rebuild (self);
}

static void
ensure_renderer (MarkerRichView *self)
{
  if (self->renderer != NULL) return;
  self->renderer = g_object_ref_sink (marker_preview_new ());
  gtk_widget_set_opacity (GTK_WIDGET (self->renderer), 0);
  gtk_widget_set_can_target (GTK_WIDGET (self->renderer), FALSE);
  gtk_widget_set_focusable (GTK_WIDGET (self->renderer), FALSE);
  gtk_accessible_update_state (GTK_ACCESSIBLE (self->renderer), GTK_ACCESSIBLE_STATE_HIDDEN, TRUE, -1);
  gtk_widget_set_size_request (GTK_WIDGET (self->renderer), self->width, 400);
  webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (self->renderer), 1.0);
  gtk_widget_set_parent (GTK_WIDGET (self->renderer), self->layer);
  g_object_set_data (G_OBJECT (self->renderer), "rich-x", GINT_TO_POINTER (-10000));
  g_object_set_data (G_OBJECT (self->renderer), "rich-y", GINT_TO_POINTER (-10000));
  g_signal_connect_object (self->renderer, "render-complete", G_CALLBACK (render_complete), self, 0);
  g_signal_connect_object (self->renderer, "gnuplot-data-changed", G_CALLBACK (data_changed), self, 0);
}

static void
marker_rich_view_dispose (GObject *object)
{
  MarkerRichView *self = MARKER_RICH_VIEW (object);
  if (!self->disposed)
    {
      self->disposed = TRUE;
      invalidate (self);
      g_clear_handle_id (&self->layout_idle, g_source_remove);
      if (self->source != NULL)
        {
          g_signal_handlers_disconnect_by_data (self->source, self);
          g_signal_handlers_disconnect_by_data (source_buffer (self), self);
          clear_items (self);
          if (self->hidden != NULL)
            gtk_text_tag_table_remove (gtk_text_buffer_get_tag_table (source_buffer (self)), self->hidden);
          if (self->renderer != NULL) gtk_widget_unparent (GTK_WIDGET (self->renderer));
        }
      g_clear_object (&self->renderer);
      g_clear_object (&self->source);
    }
  G_OBJECT_CLASS (marker_rich_view_parent_class)->dispose (object);
}

static void
marker_rich_view_finalize (GObject *object)
{
  MarkerRichView *self = MARKER_RICH_VIEW (object);
  g_ptr_array_unref (self->items);
  g_array_unref (self->rects);
  g_clear_object (&self->cancellable);
  g_free (self->markdown); g_free (self->token);
  g_free (self->theme); g_free (self->path); g_free (self->notebook);
  G_OBJECT_CLASS (marker_rich_view_parent_class)->finalize (object);
}

static void
marker_rich_view_class_init (MarkerRichViewClass *klass)
{
  g_signal_new ("source-requested", G_TYPE_FROM_CLASS (klass), G_SIGNAL_RUN_LAST,
                0, NULL, NULL, NULL, G_TYPE_NONE, 0);
  G_OBJECT_CLASS (klass)->dispose = marker_rich_view_dispose;
  G_OBJECT_CLASS (klass)->finalize = marker_rich_view_finalize;
}

static void
marker_rich_view_init (MarkerRichView *self)
{
  self->items = g_ptr_array_new_with_free_func (item_free);
  self->rects = g_array_new (FALSE, FALSE, sizeof (GdkRectangle));
  self->cancellable = g_cancellable_new ();
  self->token = g_uuid_string_random ();
}

MarkerRichView *
marker_rich_view_new (MarkerSourceView *source)
{
  MarkerRichView *self = g_object_new (MARKER_TYPE_RICH_VIEW, NULL);
  self->source = g_object_ref (source);
  self->layer = g_object_new (rich_layer_get_type (), NULL);
  gtk_text_view_add_overlay (GTK_TEXT_VIEW (source), self->layer, 0, 0);
  g_signal_connect_object (source_buffer (self), "changed", G_CALLBACK (buffer_changed), self, 0);
  g_signal_connect_object (source, "cursor-changed", G_CALLBACK (cursor_changed), self, 0);
  g_signal_connect_object (source, "layout-changed", G_CALLBACK (queue_layout), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (gtk_scrollable_get_hadjustment (GTK_SCROLLABLE (source)),
                           "value-changed", G_CALLBACK (scroll_changed), self, G_CONNECT_SWAPPED);
  g_signal_connect_object (gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (source)),
                           "value-changed", G_CALLBACK (scroll_changed), self, G_CONNECT_SWAPPED);
  return self;
}
