/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once
#include <glib.h>

/* Shared author cascade and document origin for preview, HTML and print. */
char *marker_render_style_build (const char *theme, const char *notebook_folder,
                                const char *document_folder);
