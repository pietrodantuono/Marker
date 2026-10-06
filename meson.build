project('Marker', 'c',
  version : '2020.04.04',
  license : 'GPL3'
)

PREFIX = get_option('prefix')
DATA_DIR = join_paths(PREFIX, 'share')
LIBS_DIR = join_paths(PREFIX, 'lib')
APP_DIR = join_paths(DATA_DIR, 'com.github.fabiocolacio.marker')
ICONS_DIR = join_paths(APP_DIR, 'icons')
STYLES_DIR = join_paths(APP_DIR, 'styles')
COMMON_DIR = join_paths(APP_DIR, 'common')
SCRIPTS_DIR = join_paths(APP_DIR, 'scripts')
HIGHLIGHT_STYLES_DIR = join_paths(join_paths(SCRIPTS_DIR, 'highlight'),'styles')
APPDATA_DIR = join_paths(DATA_DIR, 'metainfo')
LOCALE_DIR = join_paths(PREFIX, get_option('localedir'))

EXECUTABLE_NAME = 'marker'

gnome = import('gnome')

i18n = import('i18n')
subdir('help')
subdir('po')
subdir('data')

res = gnome.compile_resources(
  'marker_resources',
  'src/resources/marker.gresource.xml',
  source_dir : 'src/resources'
)

marker_sources = [
  'src/marker-editor.c',
  'src/marker-window.c',
  'src/marker-exporter.c',
  'src/marker-markdown.c',
  'src/marker-render-style.c',
  'src/marker-prefs.c',
  'src/marker-source-view.c',
  'src/marker-preview.c',
  'src/marker-gnuplot.c',
  'src/marker-workspace.c',
  'src/marker-project.c',
  'src/marker-notebook-icon.c',
  'src/marker-notebook-editor.c',
  'src/marker-notebook-rail.c',
  'src/marker-page-sidebar.c',
  'src/marker-session.c',
  'src/marker-formatted.c',
  'src/marker-rich-view.c',
  'src/marker-markdown-structure.c',
  'src/marker-heading-gutter.c',
  'src/marker-line-gutter.c',
  'src/marker-outline.c',
  res
]

hoedown_sources = [
  'src/scidown/src/autolink.c',
  'src/scidown/src/buffer.c',
  'src/scidown/src/document.c',
  'src/scidown/src/escape.c',
  'src/scidown/src/html.c',
  'src/scidown/src/utils.c',
  'src/scidown/src/latex.c',
  'src/scidown/src/html_blocks.c',
  'src/scidown/src/html_smartypants.c',
  'src/scidown/src/stack.c',
  'src/scidown/src/version.c',
  'src/scidown/src/constants.c',
  'src/scidown/src/charter/src/charter_string.c',
  'src/scidown/src/charter/src/csv_parser/csvparser.c',
  'src/scidown/src/charter/src/tinyexpr/tinyexpr.c',
  'src/scidown/src/charter/src/clist.c',
  'src/scidown/src/charter/src/parser.c',
  'src/scidown/src/charter/src/charter.c',
  'src/scidown/src/charter/src/svg.c',
  'src/scidown/src/charter/src/latex.c',
  'src/scidown/src/charter/src/svg_utils.c'
]

add_global_arguments(
  '-DSTYLES_DIR="@0@/"'.format(STYLES_DIR),
  '-DSCRIPTS_DIR="@0@/"'.format(SCRIPTS_DIR),
  '-DICONS_DIR="@0@/"'.format(ICONS_DIR),
  '-DHIGHLIGHT_STYLES_DIR="@0@/"'.format(HIGHLIGHT_STYLES_DIR),
  '-DMARKER_VERSION="@0@"'.format(meson.project_version()),
  '-DLOCALE_DIR="@0@"'.format(LOCALE_DIR),
  '-DCOMMON_DIR="@0@/"'.format(COMMON_DIR),
  language : 'c'
)

glib_dep = dependency('glib-2.0')
gio_dep = dependency('gio-2.0')
gtk_dep = dependency('gtk4', version : '>=4.14')
adw_dep = dependency('libadwaita-1', version : '>=1.5')
webkit_dep = dependency('webkitgtk-6.0', version : '>=2.50')
spelling_dep = dependency(
  'libspelling-1',
  version : '>=0.2.0',
  fallback : ['libspelling', 'libspelling_dep']
)

deps = [
  gtk_dep,
  adw_dep,
  glib_dep,
  gio_dep,
  dependency('gtksourceview-5', version : '>=5.12'),
  webkit_dep,
  dependency('libsoup-3.0'),
  spelling_dep
]

executable(
  EXECUTABLE_NAME,
  sources : ['src/marker.c', marker_sources, hoedown_sources],
  dependencies : deps,
  link_args: '-lm',
  install : true
)

gnuplot_test = executable(
  'marker-gnuplot-test',
  sources : ['test/marker-gnuplot-test.c', 'src/marker-gnuplot.c'],
  include_directories : include_directories('src'),
  dependencies : [glib_dep, gio_dep]
)

test('gnuplot data file boundary', gnuplot_test)

workspace_test = executable(
  'marker-workspace-test',
  sources : ['test/marker-workspace-test.c', 'src/marker-workspace.c', 'src/marker-markdown-structure.c'],
  include_directories : include_directories('src'),
  dependencies : [glib_dep, gio_dep]
)

test('workspace listing', workspace_test)

formatted_test = executable(
  'marker-formatted-test',
  sources : ['test/marker-formatted-test.c', 'src/marker-formatted.c', 'src/marker-markdown-structure.c'],
  include_directories : include_directories('src'),
  dependencies : [glib_dep, gtk_dep, adw_dep]
)

test_env = environment()
test_env.set('GSETTINGS_SCHEMA_DIR', join_paths(meson.current_build_dir(), 'data'))
test('formatted Markdown', formatted_test, env : test_env)

project_test = executable(
  'marker-project-test',
  sources : ['test/marker-project-test.c', 'src/marker-project.c'],
  include_directories : include_directories('src'),
  dependencies : [glib_dep, gio_dep],
  link_args : '-lm'
)

test('project identity', project_test)

session_test = executable(
  'marker-session-test',
  sources : ['test/marker-session-test.c', 'src/marker-session.c', 'src/marker-project.c'],
  include_directories : include_directories('src'),
  dependencies : [glib_dep, gio_dep],
  link_args : '-lm'
)

test('session persistence', session_test)

notebook_test = executable(
  'marker-notebook-test',
  sources : ['test/marker-notebook-test.c', 'src/marker-project.c', 'src/marker-session.c',
             'src/marker-notebook-icon.c', 'src/marker-notebook-editor.c',
             'src/marker-notebook-rail.c', res],
  include_directories : include_directories('src'),
  dependencies : [gio_dep, gtk_dep, adw_dep],
  link_args : '-lm'
)
test('notebook GTK widgets', notebook_test, env : test_env)

# Native pointer regression is optional and never adds application dependencies.
pointer_deps = [dependency('gtk4-x11', required : false),
                dependency('x11', required : false), dependency('xtst', required : false)]
ui_deps = deps
ui_args = []
if pointer_deps[0].found() and pointer_deps[1].found() and pointer_deps[2].found()
  ui_deps += pointer_deps
  ui_args += '-DHAVE_XTEST'
endif

ui_test = executable(
  'marker-ui-test',
  sources : ['test/marker-ui-test.c', marker_sources, hoedown_sources],
  include_directories : include_directories('src'),
  dependencies : ui_deps,
  c_args : ui_args,
  link_args : '-lm'
)
ui_env = test_env
ui_env.set('GSETTINGS_BACKEND', 'memory')
ui_env.set('GTK_A11Y', 'none')
test('window workflows', ui_test, env : ui_env, timeout : 90)

install_data(
  'data/com.github.fabiocolacio.marker.svg',
  install_dir : join_paths(DATA_DIR, 'icons/hicolor/scalable/apps')
)

install_data(
  'data/com.github.fabiocolacio.marker-symbolic.svg',
  install_dir : join_paths(DATA_DIR, 'icons/hicolor/symbolic/apps')
)

install_data(
  'data/com.github.fabiocolacio.marker.desktop',
  install_dir : join_paths(DATA_DIR, 'applications')
)

install_data(
  'data/com.github.fabiocolacio.marker.gschema.xml',
  install_dir : join_paths(DATA_DIR, 'glib-2.0/schemas')
)

install_data(
  'data/com.github.fabiocolacio.marker.appdata.xml',
  install_dir : APPDATA_DIR
)

install_subdir(
  'data/styles',
  install_dir : APP_DIR
)

install_subdir(
  'data/common',
  install_dir : APP_DIR
)

install_subdir(
  'data/scripts',
  install_dir : APP_DIR
)

install_subdir(
  'data/icons',
  install_dir : APP_DIR
)

meson.add_install_script('post-install.sh')
