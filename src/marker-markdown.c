/*
 * marker-markdown.c
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
#include <stdio.h>

#include <glib.h>
#include <glib/gprintf.h>

#include "scidown/src/html.h"
#include "scidown/src/latex.h"
#include "scidown/src/document.h"
#include "scidown/src/buffer.h"

#include "marker-markdown.h"
#include "marker-prefs.h"
#include "marker-render-style.h"

char* html_header(MarkerMathJSMode    mathjs_mode,
                  MarkerHighlightMode highlight_mode,
                  MarkerMermaidMode   mermaid_mode)
{
  char* mathjs_script;
  char* mathjs_auto;
  char* mathjs_css;

  char* highlight_css;
  char* highlight_script;

  char* mermaid_script;

  char * local_highlight_css = marker_prefs_get_highlight_theme();
  MarkerMathBackEnd backend = marker_prefs_get_math_backend();


  switch (mathjs_mode) {
    default:
    case MATHJS_OFF:
      mathjs_script = g_strdup(" ");
      mathjs_css = g_strdup(" ");
      mathjs_auto = g_strdup(" ");
      break;
  case MATHJS_NET:
      if (backend == KATEX)
      {
        mathjs_css = g_strdup("<style>@import url(\"https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.9.0-alpha2/katex.min.css\") layer(marker-base);</style>");
        mathjs_script = g_strdup("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.9.0-alpha2/katex.min.js\" crossorigin=\"anonymous\"></script>");
        mathjs_auto = g_strdup("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/KaTeX/0.9.0-alpha2/contrib/auto-render.min.js\" crossorigin=\"anonymous\"></script>");
      } else
      {
        mathjs_css = g_strdup(" ");
        mathjs_script = g_strdup("<script type=\"text/javascript\" src=\"https://cdn.jsdelivr.net/npm/mathjax@2.7.1/MathJax.js?config=TeX-AMS_HTML\"></script>");
        mathjs_auto = g_strdup(" ");
      }
      break;
  case MATHJS_LOCAL:
      if (backend == KATEX)
      {
        mathjs_css = g_strdup_printf("<style>@import url(\"file://%skatex/katex.min.css\") layer(marker-base);</style>", SCRIPTS_DIR);
        mathjs_script = g_strdup_printf("<script src=\"file://%skatex/katex.min.js\"></script>", SCRIPTS_DIR);
        mathjs_auto = g_strdup_printf("<script src=\"file://%skatex/contrib/auto-render.min.js\"></script>", SCRIPTS_DIR);
      } else
      {
        mathjs_css = g_strdup(" ");
        if (g_file_test ("/usr/share/javascript/mathjax/MathJax.js", G_FILE_TEST_IS_REGULAR))
          mathjs_script = g_strdup ("<script src=\"file:///usr/share/javascript/mathjax/MathJax.js?config=TeX-AMS_HTML\"></script>");
        else
          mathjs_script = g_strdup_printf (
            "<script>window.MathJax={tex:{inlineMath:[['$','$'],['\\\\(','\\\\)']]},"
            "options:{enableMenu:false},startup:{typeset:false}};</script>"
            "<script src=\"file://%smathjax/tex-svg-full.js\"></script>", SCRIPTS_DIR);
        mathjs_auto = g_strdup(" ");
      }
      break;
  }

 switch (highlight_mode){
    default:
    case HIGHLIGHT_OFF:
      highlight_css = g_strdup(" ");
      highlight_script = g_strdup(" ");
      break;
    case HIGHLIGHT_NET:
      highlight_css = g_strdup_printf("<style>@import url(\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/9.12.0/styles/%s.min.css\") layer(marker-base);</style>",
                                      local_highlight_css);
      highlight_script = g_strdup("<script src=\"https://cdnjs.cloudflare.com/ajax/libs/highlight.js/9.12.0/highlight.min.js\"></script>");
      break;
    case HIGHLIGHT_LOCAL:
      highlight_css = g_strdup_printf("<style>@import url(\"file://%shighlight/styles/%s.css\") layer(marker-base);</style>", SCRIPTS_DIR, local_highlight_css);
      highlight_script = g_strdup_printf("<script src=\"file://%shighlight/highlight.pack.js\"></script>", SCRIPTS_DIR);
      break;
  }

  switch (mermaid_mode)
  {
    default:
    case MERMAID_OFF:
      mermaid_script = g_strdup(" ");
      break;
    case MERMAID_NET:
      mermaid_script = g_strdup("<script src=\"https://unpkg.com/mermaid@12.1.0/dist/mermaid.min.js\"></script>");
      break;
    case MERMAID_LOCAL:
      mermaid_script = g_strdup_printf("<script src=\"file://%smermaid/mermaid.min.js\"></script>", SCRIPTS_DIR);
      break;
  }

  char * buffer = g_strdup_printf("%s\n%s\n%s\n%s\n%s\n%s\n",mathjs_css, highlight_css, mathjs_script, mathjs_auto, highlight_script, mermaid_script);

  g_free(mathjs_script);
  g_free(mathjs_auto);
  g_free(mathjs_css);

  g_free(highlight_css);
  g_free(highlight_script);
  g_free(local_highlight_css);

  g_free(mermaid_script);

  return buffer;
}


char*
html_footer(MarkerMathJSMode     mathjs_mode,
            MarkerHighlightMode highlight_mode,
            MarkerMermaidMode   mermaid_mode)
{
  char * mathjs_render;
  char * highlight_render;
  char * mermaid_render;
  MarkerMathBackEnd backend = marker_prefs_get_math_backend();

  switch(mathjs_mode){
    default:
    case MATHJS_OFF:
      mathjs_render = g_strdup(" ");
      break;
    case MATHJS_NET:
    case MATHJS_LOCAL:
      mathjs_render = g_strdup_printf (
        "<script>document.querySelectorAll('pre > code.language-math, pre > code.language-tex, pre > code.language-latex')"
        ".forEach(code=>{const math=document.createElement('div');math.className='marker-math-block';"
        "math.textContent='$$\\n'+code.textContent+'\\n$$';code.parentElement.replaceWith(math);});%s</script>",
        backend == KATEX ? "renderMathInElement(document.body);" :
                          "if(window.MathJax?.Hub){MathJax.Hub.Config({tex2jax:{inlineMath:[['$','$'],['\\\\(','\\\\)']]}});"
                          "MathJax.Hub.Queue(['Typeset',MathJax.Hub]);}"
                          "else if(window.MathJax?.startup)window.markerMathReady=MathJax.startup.promise.then(()=>MathJax.typesetPromise());");
      break;
  }

  switch(highlight_mode)
  {
    default:
    case HIGHLIGHT_OFF:
      highlight_render = g_strdup(" ");
      break;
    case HIGHLIGHT_NET:
    case HIGHLIGHT_LOCAL:
      highlight_render = g_strdup("<script>hljs.initHighlightingOnLoad();</script>");
      break;
  }

  switch(mermaid_mode)
  {
    default:
    case MERMAID_OFF:
      mermaid_render = g_strdup(" ");
      break;
    case MERMAID_NET:
    case MERMAID_LOCAL:
      mermaid_render = g_strdup("<script>mermaid.initialize({startOnLoad:false,layout:'dagre',theme:'default',look:'classic'});window.markerMermaidReady=mermaid.run();</script>");
      break;
  }

  char* buffer = g_strdup_printf("%s\n%s\n%s\n"
    "<script>window.markerRenderingReady=Promise.all([document.fonts.ready,"
    "window.markerMermaidReady||Promise.resolve(),window.markerMathReady||Promise.resolve(),new Promise(resolve=>{"
    "if(window.MathJax?.Hub)MathJax.Hub.Queue(resolve);else resolve();})]);</script>\n",
    mathjs_render, highlight_render, mermaid_render);
  g_free(highlight_render);
  g_free(mathjs_render);
  g_free(mermaid_render);
  return buffer;
}

scidown_render_flags
get_render_mode(MarkerMermaidMode mermaid_mode)
{
  scidown_render_flags mode = 0;

  if (mermaid_mode != MERMAID_OFF)
  {
    mode |= SCIDOWN_RENDER_MERMAID;
  }

  if (marker_prefs_get_use_charter())
  {
    mode |= SCIDOWN_RENDER_CHARTER;
  }

  return mode;
}

localization get_local()
{
  localization local;
  local.figure = "Figure";
  local.listing = "Listing";
  local.table = "Table";
  return local;
}

char*
marker_markdown_to_html(const char*         markdown,
                        size_t              size,
                        char *              base_folder,
                        MarkerMathJSMode     katex_mode,
                        MarkerHighlightMode highlight_mode,
                        MarkerMermaidMode   mermaid_mode,
                        const char*         stylesheet_location,
                        const char*         notebook_folder,
                        int                 cursor_position)
{
  char* html = NULL;

  hoedown_renderer* renderer;
  hoedown_document* document;
  hoedown_buffer* buffer;
  scidown_render_flags html_mode = get_render_mode(mermaid_mode);

  renderer = hoedown_html_renderer_new(html_mode, 0, get_local());

  char * header = html_header(katex_mode, highlight_mode, mermaid_mode);

  g_autofree char *styles = marker_render_style_build (stylesheet_location, notebook_folder, base_folder);
  char *scripts = header;
  header = g_strconcat (styles, scripts, NULL);
  g_free (scripts);

  char * footer = html_footer(katex_mode, highlight_mode, mermaid_mode);

  ext_definition def = {header, footer};


  document = hoedown_document_new(renderer,
                                  HOEDOWN_EXT_BLOCK         |
                                  HOEDOWN_EXT_SPAN          |
                                  HOEDOWN_EXT_FLAGS,
                                  &def,
                                  base_folder,
                                  16);

  buffer = hoedown_buffer_new(500);
  hoedown_document_render(document, buffer, (uint8_t*) markdown, size, cursor_position);

  g_free(header);
  g_free(footer);

  const char* buf_cstr = hoedown_buffer_cstr(buffer);

  html = strdup(buf_cstr);
  hoedown_html_renderer_free(renderer);
  hoedown_document_free(document);
  hoedown_buffer_free(buffer);

  return html;
}

char*
marker_markdown_to_latex(const char*         markdown,
                         size_t              size,
                         char *              base_folder,
                         MarkerMathJSMode     katex_mode,
                         MarkerHighlightMode highlight_mode,
                         MarkerMermaidMode   mermaid_mode,
                         const char*         stylesheet_location)
{
  char* latex = NULL;

  hoedown_renderer* renderer;
  hoedown_document* document;
  hoedown_buffer* buffer;
  scidown_render_flags mode = get_render_mode(mermaid_mode);

  renderer = scidown_latex_renderer_new(mode, 0, get_local());

  ext_definition def = {NULL, NULL};


  document = hoedown_document_new(renderer,
                                  HOEDOWN_EXT_BLOCK         |
                                  HOEDOWN_EXT_SPAN          |
                                  HOEDOWN_EXT_FLAGS,
                                  &def,
                                  base_folder,
                                  16);

  buffer = hoedown_buffer_new(500);
  hoedown_document_render(document, buffer, (uint8_t*) markdown, size, -1);

  const char* buf_cstr = hoedown_buffer_cstr(buffer);
  latex = strdup(buf_cstr);
  hoedown_html_renderer_free(renderer);
  hoedown_document_free(document);
  hoedown_buffer_free(buffer);

  return latex;
}


void
marker_markdown_to_latex_file(const char*         markdown,
                              size_t              size,
                              char               *base_folder,
                              MarkerMathJSMode     katex_mode,
                              MarkerHighlightMode highlight_mode,
                              MarkerMermaidMode   mermaid_mode,
                              const char*         filepath)
{


  char* latex = marker_markdown_to_latex(markdown,
                                         size,
                                         base_folder,
                                         katex_mode,
                                         highlight_mode,
                                         mermaid_mode,
                                         NULL);
  FILE* fp = fopen(filepath, "w");
  if (fp && latex)
  {
    fputs(latex, fp);
    fclose(fp);
  }

  free(latex);
}

metadata*
marker_markdown_metadata                     (const char         *markdown,
                                              size_t              size)
{
  return document_metadata((uint8_t*)markdown, size);
}
