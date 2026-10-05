/*
 * marker-preview.c
 *
 * Copyright (C) 2017-2020 - 2018 Fabio Colacio
 *
 * Marker is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * Marker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with Marker; see the file LICENSE.md. If not,
 * see <http://www.gnu.org/licenses/>.
 *
 */

#include <string.h>
#include <stdlib.h>
#include <libintl.h>

#include <glib.h>
#include <libsoup/soup.h>
#include <time.h>

#include "marker-gnuplot.h"
#include "marker-markdown-structure.h"
#include "marker-markdown.h"
#include "marker-prefs.h"


#include "marker-preview.h"
#include "marker.h"

#define MAX_ZOOM  4.0
#define MIN_ZOOM  0.1

#define SCROLL_STEP 25
#define SCROLL_STEP_SCRIPT "window.scrollBy(%d,%d);"
#define SCROLL_SCRIPT "window.scrollTo(%d,%d);"

#define GNUPLOT_WORLD "marker-gnuplot"
#define GNUPLOT_DATA_HANDLER "markerGnuplotData"
#define GNUPLOT_DONE_HANDLER "markerGnuplotDone"

struct _MarkerPreview
{
  WebKitWebView parent_instance;

  gchar      *document_dir;
  gchar      *gnuplot_script;
  GHashTable *data_monitors;
  GError     *render_error;
  guint       data_change_source;
  gboolean    gnuplot_available;
  gboolean    gnuplot_pending;
  gboolean    manual_gnuplot;
  gboolean    scroll_requested;
  gboolean    load_pending;
  gboolean    rendered;
  guint       render_serial;
};

G_DEFINE_TYPE(MarkerPreview, marker_preview, WEBKIT_TYPE_WEB_VIEW)

enum {
  GNUPLOT_DATA_CHANGED,
  RENDER_COMPLETE,
  LAST_SIGNAL
};

static guint preview_signals[LAST_SIGNAL];

static void
finish_render (MarkerPreview *preview)
{
  if (preview->load_pending || preview->gnuplot_pending || preview->rendered)
    return;
  preview->rendered = TRUE;
  if (preview->scroll_requested)
    marker_preview_scroll_to_cursor (preview);
  g_signal_emit (preview, preview_signals[RENDER_COMPLETE], 0);
}

static void
gnuplot_scheme_request_cb (WebKitURISchemeRequest *request,
                           gpointer                user_data)
{
  const gchar *path = webkit_uri_scheme_request_get_path (request);
  const gchar *filename = NULL;
  const gchar *content_type = NULL;
  g_autofree gchar *asset_path = NULL;
  gchar *contents = NULL;
  gsize length = 0;

  const gchar *method = webkit_uri_scheme_request_get_http_method (request);
  if (method != NULL && !g_str_equal (method, "GET")) {
    g_autoptr (GError) error = g_error_new_literal (G_IO_ERROR,
                                                    G_IO_ERROR_NOT_SUPPORTED,
                                                    "Unsupported gnuplot asset request.");
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  if (g_strcmp0 (path, "/gnuplot.mjs") == 0) {
    filename = "gnuplot.mjs";
    content_type = "text/javascript";
  } else if (g_strcmp0 (path, "/gnuplot.wasm") == 0) {
    filename = "gnuplot.wasm";
    content_type = "application/wasm";
  } else {
    g_autoptr (GError) error = g_error_new_literal (G_IO_ERROR,
                                                    G_IO_ERROR_NOT_FOUND,
                                                    "Unknown gnuplot asset.");
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  asset_path = g_build_filename (SCRIPTS_DIR, "gnuplot", filename, NULL);
  if (!g_file_get_contents (asset_path, &contents, &length, NULL)) {
    g_autoptr (GError) error = g_error_new_literal (G_IO_ERROR,
                                                    G_IO_ERROR_NOT_FOUND,
                                                    "The bundled gnuplot runtime is unavailable.");
    webkit_uri_scheme_request_finish_error (request, error);
    return;
  }

  g_autoptr (GInputStream) stream = g_memory_input_stream_new_from_data (contents,
                                                                         length,
                                                                         g_free);
  g_autoptr (WebKitURISchemeResponse) response =
    webkit_uri_scheme_response_new (stream, length);
  g_autoptr (SoupMessageHeaders) headers =
    soup_message_headers_new (SOUP_MESSAGE_HEADERS_RESPONSE);
  soup_message_headers_append (headers, "Access-Control-Allow-Origin", "*");
  webkit_uri_scheme_response_set_status (response, 200, "OK");
  webkit_uri_scheme_response_set_content_type (response, content_type);
  webkit_uri_scheme_response_set_http_headers (response,
                                                g_steal_pointer (&headers));
  webkit_uri_scheme_request_finish_with_response (request, response);
}

static void
register_gnuplot_scheme (void)
{
  static gsize registered = 0;

  if (g_once_init_enter (&registered)) {
    WebKitWebContext *context = webkit_web_context_get_default ();
    WebKitSecurityManager *security = webkit_web_context_get_security_manager (context);

    webkit_web_context_register_uri_scheme (context,
                                            "marker-gnuplot",
                                            gnuplot_scheme_request_cb,
                                            NULL,
                                            NULL);
    webkit_security_manager_register_uri_scheme_as_local (security, "marker-gnuplot");
    webkit_security_manager_register_uri_scheme_as_secure (security, "marker-gnuplot");
    webkit_security_manager_register_uri_scheme_as_cors_enabled (security,
                                                                 "marker-gnuplot");
    g_once_init_leave (&registered, 1);
  }
}

static void
data_monitor_free (gpointer data)
{
  GFileMonitor *monitor = data;
  g_file_monitor_cancel (monitor);
  g_object_unref (monitor);
}

static gboolean
emit_data_changed_cb (gpointer user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (user_data);
  preview->data_change_source = 0;
  g_signal_emit (preview, preview_signals[GNUPLOT_DATA_CHANGED], 0);
  return G_SOURCE_REMOVE;
}

static void
data_file_changed_cb (GFileMonitor      *monitor,
                      GFile             *file,
                      GFile             *other_file,
                      GFileMonitorEvent  event,
                      gpointer           user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (user_data);

  if (preview->data_change_source == 0)
    preview->data_change_source = g_timeout_add (50, emit_data_changed_cb, preview);
}

static void
monitor_data_file (MarkerPreview *preview,
                   GFile         *file)
{
  g_autofree gchar *path = g_file_get_path (file);
  g_autoptr (GError) error = NULL;
  GFileMonitor *monitor;

  if (path == NULL || g_hash_table_contains (preview->data_monitors, path))
    return;

  monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, &error);
  if (monitor == NULL)
    return;

  g_signal_connect_object (monitor,
                           "changed",
                           G_CALLBACK (data_file_changed_cb),
                           preview,
                           0);
  g_hash_table_insert (preview->data_monitors, g_steal_pointer (&path), monitor);
}

static void
finish_gnuplot_render (MarkerPreview *preview)
{
  if (!preview->gnuplot_pending)
    return;

  preview->gnuplot_pending = FALSE;
  finish_render (preview);
}

static gboolean
gnuplot_data_message_cb (WebKitUserContentManager *manager,
                         JSCValue                 *value,
                         WebKitScriptMessageReply *reply,
                         gpointer                  user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (user_data);
  g_autofree gchar *operand = NULL;
  g_autofree gchar *encoded = NULL;
  g_autoptr (GBytes) bytes = NULL;
  g_autoptr (GFile) file = NULL;
  g_autoptr (GError) error = NULL;

  if (!jsc_value_is_object (value))
    {
      webkit_script_message_reply_return_error_message (reply, "Invalid plot data request.");
      return TRUE;
    }
  g_autoptr (JSCValue) serial = jsc_value_object_get_property (value, "serial");
  g_autoptr (JSCValue) path = jsc_value_object_get_property (value, "path");
  if (!jsc_value_is_string (path) || jsc_value_to_int32 (serial) != preview->render_serial) {
    webkit_script_message_reply_return_error_message (reply, "Invalid plot data path.");
    return TRUE;
  }

  operand = jsc_value_to_string (path);
  bytes = marker_gnuplot_load_data (preview->document_dir, operand, &file, &error);
  if (file != NULL)
    monitor_data_file (preview, file);
  if (bytes == NULL) {
    webkit_script_message_reply_return_error_message (reply, error->message);
    return TRUE;
  }

  gsize length;
  const guchar *contents = g_bytes_get_data (bytes, &length);
  encoded = g_base64_encode (contents, length);
  g_autoptr (JSCValue) response =
    jsc_value_new_string (jsc_value_get_context (value), encoded);
  webkit_script_message_reply_return_value (reply, response);
  return TRUE;
}

static gboolean
gnuplot_done_message_cb (WebKitUserContentManager *manager,
                         JSCValue                 *value,
                         WebKitScriptMessageReply *reply,
                         gpointer                  user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (user_data);

  if (jsc_value_to_int32 (value) == preview->render_serial)
    finish_gnuplot_render (preview);

  g_autoptr (JSCValue) response =
    jsc_value_new_boolean (jsc_value_get_context (value), TRUE);
  webkit_script_message_reply_return_value (reply, response);
  return TRUE;
}

static gboolean
open_uri (WebKitPolicyDecision *decision) {
  WebKitNavigationPolicyDecision *nav_dec = WEBKIT_NAVIGATION_POLICY_DECISION(decision);
  WebKitNavigationAction *action = webkit_navigation_policy_decision_get_navigation_action (nav_dec);
  WebKitURIRequest *request = webkit_navigation_action_get_request (action);
  const gchar *uri = webkit_uri_request_get_uri (request);
  if (g_str_has_prefix (uri, "http://") ||
      g_str_has_prefix (uri, "https://") ||
      g_str_has_prefix (uri, "mailto:")) {
    GtkApplication *app = marker_get_app ();
    GtkUriLauncher *launcher = gtk_uri_launcher_new (uri);

    gtk_uri_launcher_launch (launcher,
                             gtk_application_get_active_window (app),
                             NULL, NULL, NULL);
    g_object_unref (launcher);
    webkit_policy_decision_ignore(decision);
    return TRUE;
  }
  return FALSE;
}

static gboolean
navigate(WebKitPolicyDecision *decision)
{
  /** TODO FIX internal navigation
  WebKitNavigationPolicyDecision * nav_dec = WEBKIT_NAVIGATION_POLICY_DECISION(decision);

  const gchar * uri =webkit_uri_request_get_uri(webkit_navigation_action_get_request (webkit_navigation_policy_decision_get_navigation_action (nav_dec)));
  g_print(">> %s\n", uri);
  **/

  /* if request is http ignore default policy and open uri in default browser*/
  return open_uri (decision);
}

static gboolean
decide_policy_cb (WebKitWebView *web_view,
                  WebKitPolicyDecision *decision,
                  WebKitPolicyDecisionType type)
{
    switch (type) {
    case WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION:
      return navigate(decision);
    case WEBKIT_POLICY_DECISION_TYPE_RESPONSE:
      webkit_policy_decision_use (decision);
      break;
    case WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION:
      return navigate(decision);
    default:  
      /* Making no decision results in webkit_policy_decision_use(). */
      return FALSE;
    }
    return TRUE;
}


static gboolean
context_menu_cb  (WebKitWebView       *web_view,
                  WebKitContextMenu   *context_menu,
                  WebKitHitTestResult *hit_test_result,
                  gpointer             user_data)
{
  return TRUE;
}

static void
set_render_error (MarkerPreview *preview,
                  const GError  *error)
{
  g_clear_error (&preview->render_error);
  preview->render_error = error != NULL
    ? g_error_copy (error)
    : g_error_new_literal (G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Gnuplot rendering failed.");
  preview->load_pending = FALSE;
  finish_gnuplot_render (preview);
  finish_render (preview);
}

static void
manual_gnuplot_finished_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (object);
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value =
    webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (object),
                                                result,
                                                &error);
  if (value == NULL && GPOINTER_TO_UINT (user_data) == preview->render_serial)
    set_render_error (preview, error);
}

static gboolean
load_failed_cb (WebKitWebView  *web_view,
                WebKitLoadEvent event,
                const gchar    *failing_uri,
                GError         *error,
                gpointer        user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (web_view);
  if ((preview->gnuplot_pending || preview->load_pending) &&
      !g_error_matches (error, WEBKIT_NETWORK_ERROR, WEBKIT_NETWORK_ERROR_CANCELLED))
    set_render_error (preview, error);
  return FALSE;
}

static void
web_process_terminated_cb (WebKitWebView                    *web_view,
                           WebKitWebProcessTerminationReason reason,
                           gpointer                          user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (web_view);

  if (preview->gnuplot_pending || preview->load_pending) {
    g_autoptr (GError) error =
      g_error_new_literal (G_IO_ERROR,
                           G_IO_ERROR_FAILED,
                           "The preview web process stopped.");
    set_render_error (preview, error);
  }
}

static gboolean
key_pressed_cb (GtkEventControllerKey *controller,
                guint                  keyval,
                guint                  keycode,
                GdkModifierType        state,
                gpointer               user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (user_data);

  if ((state & GDK_CONTROL_MASK) != 0)
  {
    switch (keyval)
    {
      case GDK_KEY_plus:
        marker_preview_zoom_in (preview);
        break;

      case GDK_KEY_minus:
        marker_preview_zoom_out (preview);
        break;

      case GDK_KEY_0:
        marker_preview_zoom_original (preview);
        break;
    }
  }
  else
  {
    switch (keyval)
    {
      case GDK_KEY_j:
        marker_preview_scroll_down (preview);
        break;

      case GDK_KEY_k:
        marker_preview_scroll_up (preview);
        break;

      case GDK_KEY_h:
        marker_preview_scroll_left (preview);
        break;

      case GDK_KEY_l:
        marker_preview_scroll_right (preview);
        break;

      case GDK_KEY_g:
        marker_preview_scroll_to_top (preview);
        break;

      case GDK_KEY_G:
        marker_preview_scroll_to_bottom (preview);
        break;
    }
  }

  return FALSE;
}

static gboolean
scroll_cb (GtkEventControllerScroll *controller,
           gdouble                   delta_x,
           gdouble                   delta_y,
           gpointer                  user_data)
{
  MarkerPreview *preview = MARKER_PREVIEW (user_data);
  GdkModifierType state = gtk_event_controller_get_current_event_state (
    GTK_EVENT_CONTROLLER (controller));
  if ((state & GDK_CONTROL_MASK) != 0)
  {
    if (delta_y > 0)
    {
      marker_preview_zoom_out (preview);
    }
    else if (delta_y < 0)
    {
      marker_preview_zoom_in (preview);
    }
  }

  return FALSE;
}

static void
resources_ready (GObject *object, GAsyncResult *result, gpointer data)
{
  MarkerPreview *preview = MARKER_PREVIEW (object);
  guint serial = GPOINTER_TO_UINT (data);
  g_autoptr (GError) error = NULL;
  g_autoptr (JSCValue) value = webkit_web_view_call_async_javascript_function_finish (WEBKIT_WEB_VIEW (object), result, &error);
  if (serial != preview->render_serial)
    return;
  if (value == NULL)
    {
      set_render_error (preview, error);
      return;
    }
  if (jsc_value_to_int32 (value) != serial)
    return;
  preview->load_pending = FALSE;
  finish_render (preview);
}

static void
load_changed_cb (WebKitWebView   *web_view,
                 WebKitLoadEvent  event)
{
  MarkerPreview *preview = MARKER_PREVIEW (web_view);

  switch (event)
  {
    case WEBKIT_LOAD_STARTED:
      break;

    case WEBKIT_LOAD_REDIRECTED:
      break;

    case WEBKIT_LOAD_COMMITTED:
      break;

    case WEBKIT_LOAD_FINISHED:
      if (preview->manual_gnuplot) {
        preview->manual_gnuplot = FALSE;
        webkit_settings_set_enable_javascript (
          webkit_web_view_get_settings (web_view), TRUE);
        if (preview->gnuplot_script != NULL) {
          g_autofree char *script = g_strdup_printf ("globalThis.markerRenderSerial = %u;\n%s",
                                                     preview->render_serial, preview->gnuplot_script);
          webkit_web_view_evaluate_javascript (web_view,
                                               script,
                                               -1,
                                               GNUPLOT_WORLD,
                                               NULL,
                                               NULL,
                                               manual_gnuplot_finished_cb,
                                               GUINT_TO_POINTER (preview->render_serial));
        } else {
          g_autoptr (GError) error =
            g_error_new_literal (G_IO_ERROR,
                                 G_IO_ERROR_NOT_FOUND,
                                 "The bundled gnuplot preview adapter is unavailable.");
          set_render_error (preview, error);
        }
      }
      webkit_web_view_call_async_javascript_function (web_view,
        "await (window.markerRenderingReady || Promise.resolve());"
        "await document.fonts.ready;"
        "return Number(document.querySelector('meta[name=marker-render]')?.content || 0);",
        -1, NULL, NULL, NULL, NULL, resources_ready, GUINT_TO_POINTER (preview->render_serial));
      break;
  }
}

static void
pdf_print_failed_cb (WebKitPrintOperation* print_op,
                     GError*               err,
                     gpointer              user_data)
{
  g_printerr("print failed with error: %s\n", err->message);
}

typedef struct {
  GMainLoop *loop;
  gboolean   complete;
  gboolean   failed;
} PrintWait;

static void
pdf_print_finished_cb (WebKitPrintOperation *print_op,
                       gpointer              user_data)
{
  PrintWait *wait = user_data;
  wait->complete = TRUE;
  g_main_loop_quit (wait->loop);
}

static void
pdf_print_wait_failed_cb (WebKitPrintOperation *print_op,
                          GError               *error,
                          gpointer              user_data)
{
  PrintWait *wait = user_data;
  pdf_print_failed_cb (print_op, error, NULL);
  wait->failed = TRUE;
}

static void
scroll_js_finished_cb (GObject      *object,
                       GAsyncResult *result,
                       gpointer      user_data)
{
  g_autoptr (JSCValue) value = NULL;
  g_autoptr (GError) error = NULL;

  value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (object), result, &error);
  if (error != NULL) {
    g_debug ("Error running preview script: %s", error->message);
    return;
  }
}


static void
marker_preview_init (MarkerPreview *preview)
{
  GtkEventController *key_controller = gtk_event_controller_key_new ();
  GtkEventController *scroll_controller =
    gtk_event_controller_scroll_new (GTK_EVENT_CONTROLLER_SCROLL_VERTICAL);

  preview->data_monitors = g_hash_table_new_full (g_str_hash,
                                                   g_str_equal,
                                                   g_free,
                                                   data_monitor_free);
  g_signal_connect (key_controller, "key-pressed", G_CALLBACK (key_pressed_cb), preview);
  g_signal_connect (scroll_controller, "scroll", G_CALLBACK (scroll_cb), preview);
  gtk_widget_add_controller (GTK_WIDGET (preview), key_controller);
  gtk_widget_add_controller (GTK_WIDGET (preview), scroll_controller);
  g_signal_connect (preview, "decide-policy", G_CALLBACK (decide_policy_cb), NULL);
  g_signal_connect (preview, "context-menu", G_CALLBACK (context_menu_cb), NULL);
  g_signal_connect (preview, "load-failed", G_CALLBACK (load_failed_cb), NULL);
  g_signal_connect (preview,
                    "web-process-terminated",
                    G_CALLBACK (web_process_terminated_cb),
                    NULL);
}

static void
marker_preview_dispose (GObject *object)
{
  MarkerPreview *preview = MARKER_PREVIEW (object);

  if (preview->data_change_source != 0) {
    g_source_remove (preview->data_change_source);
    preview->data_change_source = 0;
  }
  g_hash_table_remove_all (preview->data_monitors);

  G_OBJECT_CLASS (marker_preview_parent_class)->dispose (object);
}

static void
marker_preview_finalize (GObject *object)
{
  MarkerPreview *preview = MARKER_PREVIEW (object);

  g_clear_pointer (&preview->document_dir, g_free);
  g_clear_pointer (&preview->gnuplot_script, g_free);
  g_clear_pointer (&preview->data_monitors, g_hash_table_unref);
  g_clear_error (&preview->render_error);

  G_OBJECT_CLASS (marker_preview_parent_class)->finalize (object);
}

static void
marker_preview_class_init (MarkerPreviewClass *class)
{
  GObjectClass *object_class = G_OBJECT_CLASS (class);

  object_class->dispose = marker_preview_dispose;
  object_class->finalize = marker_preview_finalize;


  preview_signals[GNUPLOT_DATA_CHANGED] =
    g_signal_new ("gnuplot-data-changed",
                  G_TYPE_FROM_CLASS (class),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL, NULL,
                  G_TYPE_NONE, 0);
  preview_signals[RENDER_COMPLETE] = g_signal_new ("render-complete", G_TYPE_FROM_CLASS (class),
    G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);


  WEBKIT_WEB_VIEW_CLASS(class)->load_changed = load_changed_cb;
}

MarkerPreview*
marker_preview_new(void)
{
  g_autoptr (WebKitUserContentManager) manager = NULL;
  g_autofree gchar *script_path = NULL;
  MarkerPreview *obj;

  register_gnuplot_scheme ();
  manager = webkit_user_content_manager_new ();
  obj = g_object_new (MARKER_TYPE_PREVIEW,
                      "user-content-manager", manager,
                      NULL);

  script_path = g_build_filename (SCRIPTS_DIR,
                                  "gnuplot",
                                  "gnuplot-preview.js",
                                  NULL);
  if (g_file_get_contents (script_path, &obj->gnuplot_script, NULL, NULL)) {
    obj->gnuplot_available =
      webkit_user_content_manager_register_script_message_handler_with_reply (
        manager, GNUPLOT_DATA_HANDLER, GNUPLOT_WORLD) &&
      webkit_user_content_manager_register_script_message_handler_with_reply (
        manager, GNUPLOT_DONE_HANDLER, GNUPLOT_WORLD);
  }

  g_signal_connect_object (manager,
                           "script-message-with-reply-received::" GNUPLOT_DATA_HANDLER,
                           G_CALLBACK (gnuplot_data_message_cb),
                           obj,
                           0);
  g_signal_connect_object (manager,
                           "script-message-with-reply-received::" GNUPLOT_DONE_HANDLER,
                           G_CALLBACK (gnuplot_done_message_cb),
                           obj,
                           0);

  webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (obj), marker_prefs_get_zoom_level ());
  

  /***
  WebKitSettings * settings = webkit_web_view_get_settings(WEBKIT_WEB_VIEW(obj));
  webkit_settings_set_enable_write_console_messages_to_stdout(settings, TRUE);
  webkit_web_view_set_settings(WEBKIT_WEB_VIEW(obj), settings);
  ***/

  return obj;
}

void
marker_preview_zoom_out (MarkerPreview *preview)
{
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (preview));
  WebKitWebView *view = WEBKIT_WEB_VIEW (preview);

  gdouble val = webkit_web_view_get_zoom_level (view) - 0.1;
  val = MAX (val, MIN_ZOOM);

  marker_prefs_set_zoom_level(val);
  webkit_web_view_set_zoom_level(view, val);

}

void
marker_preview_zoom_original (MarkerPreview *preview)
{
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (preview));
  WebKitWebView *view = WEBKIT_WEB_VIEW (preview);

  gdouble zoom = 1.0;

  marker_prefs_set_zoom_level (zoom);
  webkit_web_view_set_zoom_level (view, zoom);

}

void
marker_preview_zoom_in (MarkerPreview *preview)
{
  g_return_if_fail (WEBKIT_IS_WEB_VIEW (preview));
  WebKitWebView *view = WEBKIT_WEB_VIEW (preview);

  gdouble val = webkit_web_view_get_zoom_level (view) + 0.1;
  val = MIN (val, MAX_ZOOM);

  marker_prefs_set_zoom_level(val);
  webkit_web_view_set_zoom_level(view, val);

}

static void
marker_preview_load_html (MarkerPreview *preview,
                          const gchar   *html,
                          const gchar   *document_path,
                          gboolean       manual_gnuplot)
{
  g_autofree gchar *uri = NULL;
  WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (preview));
  gboolean gnuplot_enabled = marker_prefs_get_use_gnuplot () && preview->gnuplot_available;
  preview->render_serial++;
  preview->load_pending = TRUE;
  preview->rendered = FALSE;
  webkit_user_content_manager_remove_all_scripts (manager);
  g_autofree char *serial_script = g_strdup_printf ("globalThis.markerRenderSerial = %u;", preview->render_serial);
  g_autoptr (WebKitUserScript) serial = webkit_user_script_new_for_world (
    serial_script, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME, WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_START,
    GNUPLOT_WORLD, NULL, NULL);
  webkit_user_content_manager_add_script (manager, serial);
  if (gnuplot_enabled && !manual_gnuplot) {
    g_autoptr (WebKitUserScript) script = webkit_user_script_new_for_world (
      preview->gnuplot_script, WEBKIT_USER_CONTENT_INJECT_TOP_FRAME,
      WEBKIT_USER_SCRIPT_INJECT_AT_DOCUMENT_END, GNUPLOT_WORLD, NULL, NULL);
    webkit_user_content_manager_add_script (manager, script);
  }
  GdkRGBA background;
  gdk_rgba_parse (&background, marker_prefs_get_use_dark_theme () ? "#1d1d1d" : "#ffffff");
  webkit_web_view_set_background_color (WEBKIT_WEB_VIEW (preview), &background);

  if (preview->data_change_source != 0) {
    g_source_remove (preview->data_change_source);
    preview->data_change_source = 0;
  }
  g_hash_table_remove_all (preview->data_monitors);
  g_clear_pointer (&preview->document_dir, g_free);
  g_clear_error (&preview->render_error);
  preview->document_dir = document_path != NULL
    ? g_path_get_dirname (document_path)
    : NULL;
  preview->gnuplot_pending = gnuplot_enabled && strstr (html, "language-gnuplot") != NULL;
  preview->manual_gnuplot = manual_gnuplot && gnuplot_enabled;
  preview->scroll_requested = TRUE;

  webkit_settings_set_enable_javascript (
    webkit_web_view_get_settings (WEBKIT_WEB_VIEW (preview)),
    !preview->manual_gnuplot);

  if (document_path != NULL)
    uri = g_filename_to_uri (document_path, NULL, NULL);
  if (uri == NULL)
    uri = g_filename_to_uri (g_get_home_dir (), NULL, NULL);

  g_autofree char *render_marker = g_strdup_printf ("<meta name=\"marker-render\" content=\"%u\"></head>", preview->render_serial);
  g_autoptr (GString) content = g_string_new (html);
  g_string_replace (content, "</head>", render_marker, 1);
  webkit_web_view_load_html (WEBKIT_WEB_VIEW (preview), content->str, uri);
}

void
marker_preview_render_markdown(MarkerPreview* preview,
                               const char*    markdown,
                               const char*    css_theme,
                               const char*    base_uri,
                               const char*    notebook_folder,
                               int            cursor)
{
  MarkerMathJSMode katex_mode = MATHJS_OFF;
  if (marker_prefs_get_use_mathjs()) {
    katex_mode = MATHJS_LOCAL;
  }
  MarkerHighlightMode highlight_mode = HIGHLIGHT_OFF;
  if (marker_prefs_get_use_highlight()){
    highlight_mode = HIGHLIGHT_LOCAL;
  }
  MarkerMermaidMode mermaid_mode = MERMAID_OFF;
  if (marker_prefs_get_use_mermaid())
  {
    mermaid_mode = MERMAID_LOCAL;
  }

  g_autofree char *base_folder = NULL;
  if (base_uri)
    base_folder = g_path_get_dirname (base_uri);
  else
    base_folder = g_strdup (notebook_folder != NULL ? notebook_folder : g_get_home_dir ());
  g_autofree char *anchored = cursor >= 0 ? marker_markdown_structure_cursor_source (markdown, cursor) : g_strdup (markdown);
  char* html = marker_markdown_to_html(anchored,
                                       strlen(anchored),
                                       base_folder,
                                       katex_mode,
                                       highlight_mode,
                                       mermaid_mode,
                                       css_theme,
                                       notebook_folder,
                                       -1);

  g_autofree char *render_path = base_uri != NULL ? g_strdup (base_uri)
    : g_build_filename (base_folder, ".marker-preview.html", NULL);
  marker_preview_load_html (preview, html, render_path, FALSE);
  free(html);
}

static void
render_complete_quit_cb (MarkerPreview *preview,
                         gpointer       user_data)
{
  g_main_loop_quit (user_data);
}

static gboolean
ready_timeout (gpointer data)
{
  MarkerPreview *preview = MARKER_PREVIEW (data);
  g_autoptr (GError) error = g_error_new_literal (G_IO_ERROR, G_IO_ERROR_TIMED_OUT,
    "The preview did not finish rendering within 30 seconds.");
  set_render_error (preview, error);
  return G_SOURCE_REMOVE;
}

gboolean
marker_preview_wait_ready (MarkerPreview *preview,
                                 GError       **error)
{
  if (preview->gnuplot_pending || preview->load_pending) {
    g_autoptr (GMainLoop) loop = g_main_loop_new (NULL, FALSE);
    gulong handler = g_signal_connect (preview,
                                       "render-complete",
                                       G_CALLBACK (render_complete_quit_cb),
                                       loop);
    guint timeout = g_timeout_add_seconds (30, ready_timeout, preview);
    if (preview->gnuplot_pending || preview->load_pending)
      g_main_loop_run (loop);
    if (g_main_context_find_source_by_id (NULL, timeout) != NULL)
      g_source_remove (timeout);
    g_signal_handler_disconnect (preview, handler);
  }

  if (preview->render_error != NULL) {
    g_propagate_error (error, g_error_copy (preview->render_error));
    return FALSE;
  }
  return TRUE;
}

typedef struct {
  GMainLoop *loop;
  JSCValue  *value;
  GError    *error;
  gboolean   complete;
} JavascriptWait;

static void
export_javascript_finished_cb (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  JavascriptWait *wait = user_data;
  wait->value = webkit_web_view_evaluate_javascript_finish (WEBKIT_WEB_VIEW (object),
                                                            result,
                                                            &wait->error);
  wait->complete = TRUE;
  g_main_loop_quit (wait->loop);
}

gboolean
marker_preview_export_html (const gchar  *staging_html,
                            const gchar  *final_html,
                            const gchar  *document_path,
                            const gchar  *outfile,
                            GError      **error)
{
  g_autoptr (MarkerPreview) preview = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  g_autofree gchar *encoded = NULL;
  g_autofree gchar *script = NULL;
  g_autofree gchar *rendered = NULL;
  JavascriptWait wait = {0};

  g_return_val_if_fail (staging_html != NULL, FALSE);
  g_return_val_if_fail (final_html != NULL, FALSE);
  g_return_val_if_fail (outfile != NULL, FALSE);

  preview = g_object_ref_sink (marker_preview_new ());
  if (!preview->gnuplot_available) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
                         "The bundled gnuplot preview adapter is unavailable.");
    return FALSE;
  }

  marker_preview_load_html (preview, staging_html, document_path, TRUE);
  if (!marker_preview_wait_ready (preview, error))
    return FALSE;

  encoded = g_base64_encode ((const guchar *) final_html, strlen (final_html));
  script = g_strdup_printf ("globalThis.markerGnuplotExport(\"%s\")", encoded);
  loop = g_main_loop_new (NULL, FALSE);
  wait.loop = loop;
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview),
                                       script,
                                       -1,
                                       GNUPLOT_WORLD,
                                       NULL,
                                       NULL,
                                       export_javascript_finished_cb,
                                       &wait);
  if (!wait.complete)
    g_main_loop_run (loop);

  if (wait.value == NULL) {
    if (wait.error != NULL)
      g_propagate_error (error, wait.error);
    else
      g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                           "Gnuplot export did not return a result.");
    return FALSE;
  }
  if (!jsc_value_is_string (wait.value)) {
    g_object_unref (wait.value);
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Gnuplot export did not return an HTML document.");
    return FALSE;
  }

  rendered = jsc_value_to_string (wait.value);
  g_object_unref (wait.value);
  if (rendered == NULL) {
    g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
                         "Gnuplot export did not return an HTML document.");
    return FALSE;
  }
  return g_file_set_contents (outfile, rendered, -1, error);
}

WebKitPrintOperationResponse
marker_preview_run_print_dialog(MarkerPreview* preview,
                                GtkWindow*     parent)
{
  g_autoptr (GError) error = NULL;
  WebKitPrintOperationResponse response;
  WebKitPrintOperation* print_op;

  if (!marker_preview_wait_ready (preview, &error)) {
    g_warning ("Unable to finish gnuplot preview before printing: %s", error->message);
    return WEBKIT_PRINT_OPERATION_RESPONSE_CANCEL;
  }

  print_op =
    webkit_print_operation_new(WEBKIT_WEB_VIEW(preview));

  g_signal_connect(print_op, "failed", G_CALLBACK(pdf_print_failed_cb), NULL);
  response = webkit_print_operation_run_dialog(print_op, parent);
  g_object_unref (print_op);
  return response;
}

gboolean
marker_preview_print_pdf(MarkerPreview*     preview,
                         const char*        outfile,
                         enum scidown_paper_size paper_size,
                         GtkPageOrientation orientation)

{
  g_autoptr (GError) error = NULL;
  g_autofree gchar *uri = NULL;
  g_autofree gchar *custom_name = NULL;
  g_autoptr (GMainLoop) loop = NULL;
  WebKitPrintOperation *print_op;
  GtkPrintSettings *print_settings;
  GtkPageSetup *page_setup;
  GtkPaperSize *gtk_paper_size;
  PrintWait wait = {0};

  if (!marker_preview_wait_ready (preview, &error)) {
    g_warning ("Unable to finish gnuplot preview before PDF export: %s", error->message);
    return FALSE;
  }

  uri = g_filename_to_uri (outfile, NULL, &error);
  if (uri == NULL) {
    g_warning ("Unable to create PDF output URI: %s", error->message);
    return FALSE;
  }

  print_op = webkit_print_operation_new (WEBKIT_WEB_VIEW (preview));
  print_settings = gtk_print_settings_new ();
  page_setup = gtk_page_setup_new ();
  if (paper_size == B43)
    gtk_paper_size = gtk_paper_size_new_custom ("B43", "B43", 166, 221, GTK_UNIT_MM);
  else if (paper_size == B169)
    gtk_paper_size = gtk_paper_size_new_custom ("B169", "B169", 166, 294, GTK_UNIT_MM);
  else
    gtk_paper_size = gtk_paper_size_new (paper_to_gtkstr (paper_size));

  gtk_print_settings_set (print_settings, GTK_PRINT_SETTINGS_OUTPUT_FILE_FORMAT, "pdf");
  gtk_print_settings_set (print_settings, GTK_PRINT_SETTINGS_OUTPUT_URI, uri);
  gtk_print_settings_set (print_settings, GTK_PRINT_SETTINGS_PRINTER,
                          dgettext ("gtk40", "Print to File"));

  if (orientation == GTK_PAGE_ORIENTATION_PORTRAIT) {
    gtk_page_setup_set_paper_size (page_setup, gtk_paper_size);
    gtk_print_settings_set_paper_width (
      print_settings,
      gtk_paper_size_get_width (gtk_paper_size, GTK_UNIT_MM),
      GTK_UNIT_MM);
    gtk_print_settings_set_paper_height (
      print_settings,
      gtk_paper_size_get_height (gtk_paper_size, GTK_UNIT_MM),
      GTK_UNIT_MM);
  } else {
    gdouble width = gtk_paper_size_get_width (gtk_paper_size, GTK_UNIT_MM);
    gdouble height = gtk_paper_size_get_height (gtk_paper_size, GTK_UNIT_MM);
    custom_name = g_strdup_printf ("%s_landscape", paper_to_string (paper_size));
    GtkPaperSize *custom_size = gtk_paper_size_new_custom (custom_name,
                                                           "pdf",
                                                           height,
                                                           width,
                                                           GTK_UNIT_MM);
    gtk_page_setup_set_paper_size (page_setup, custom_size);
    gtk_paper_size_free (custom_size);
    gtk_print_settings_set_paper_width (print_settings, height, GTK_UNIT_MM);
    gtk_print_settings_set_paper_height (print_settings, width, GTK_UNIT_MM);
  }
  gtk_paper_size_free (gtk_paper_size);

  if (paper_size == B43 || paper_size == B169) {
    gtk_page_setup_set_left_margin (page_setup, 0, GTK_UNIT_POINTS);
    gtk_page_setup_set_right_margin (page_setup, 0, GTK_UNIT_POINTS);
    gtk_page_setup_set_top_margin (page_setup, 0, GTK_UNIT_POINTS);
    gtk_page_setup_set_bottom_margin (page_setup, 0, GTK_UNIT_POINTS);
  }
  gtk_print_settings_set_orientation (print_settings, orientation);
  webkit_print_operation_set_print_settings (print_op, print_settings);
  webkit_print_operation_set_page_setup (print_op, page_setup);

  loop = g_main_loop_new (NULL, FALSE);
  wait.loop = loop;
  g_signal_connect (print_op, "failed", G_CALLBACK (pdf_print_wait_failed_cb), &wait);
  g_signal_connect (print_op, "finished", G_CALLBACK (pdf_print_finished_cb), &wait);
  webkit_print_operation_print (print_op);
  if (!wait.complete)
    g_main_loop_run (loop);

  g_object_unref (page_setup);
  g_object_unref (print_settings);
  g_object_unref (print_op);
  return !wait.failed;
}

void
marker_preview_scroll_left (MarkerPreview *preview)
{
  g_autofree gchar *script = g_strdup_printf (SCROLL_STEP_SCRIPT, -SCROLL_STEP, 0);
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1,
                                       NULL, NULL, NULL, scroll_js_finished_cb, NULL);
}

void
marker_preview_scroll_right (MarkerPreview *preview)
{
  g_autofree gchar *script = g_strdup_printf (SCROLL_STEP_SCRIPT, SCROLL_STEP, 0);
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1,
                                       NULL, NULL, NULL, scroll_js_finished_cb, NULL);
}

void
marker_preview_scroll_up (MarkerPreview *preview)
{
  g_autofree gchar *script = g_strdup_printf (SCROLL_STEP_SCRIPT, 0, -SCROLL_STEP);
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1,
                                       NULL, NULL, NULL, scroll_js_finished_cb, NULL);
}

void
marker_preview_scroll_down (MarkerPreview *preview)
{
  g_autofree gchar *script = g_strdup_printf (SCROLL_STEP_SCRIPT, 0, SCROLL_STEP);
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1,
                                       NULL, NULL, NULL, scroll_js_finished_cb, NULL);
}

void
marker_preview_scroll_to_top (MarkerPreview *preview)
{
  g_autofree gchar *script = g_strdup_printf (SCROLL_SCRIPT, 0, 0);
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1,
                                       NULL, NULL, NULL, scroll_js_finished_cb, NULL);
}

void
marker_preview_scroll_to_bottom (MarkerPreview *preview)
{
  const gchar *script = "window.scrollTo(0,document.body.scrollHeight);";
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1,
                                       NULL, NULL, NULL, scroll_js_finished_cb, NULL);
}

void
marker_preview_scroll_to_cursor (MarkerPreview *preview)
{
  preview->scroll_requested = TRUE;
  if (preview->load_pending || preview->gnuplot_pending)
    return;
  preview->scroll_requested = FALSE;
  const gchar *script =
    "document.getElementById('cursor_pos')?.scrollIntoView({block:'center',behavior:'smooth'});";
  webkit_web_view_evaluate_javascript (WEBKIT_WEB_VIEW (preview), script, -1,
                                       NULL, NULL, NULL, scroll_js_finished_cb, NULL);
}
