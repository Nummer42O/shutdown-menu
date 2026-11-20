## / ##

# required setup
cmake_minimum_required(VERSION 3.16)
project(shutdown-menu
  VERSION 0.2.1
  LANGUAGES C
)

# variables
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

include(GNUInstallDirs)
include(CTest)

# The application id is used for both GApplication and the GSettings schema/path.
# Keep it configurable so packagers can override it consistently.
set(SHUTDOWN_MENU_APPLICATION_ID
  "io.github.Nummer42O.ShutdownMenu"
  CACHE STRING "Reverse-DNS application id"
)
if(NOT SHUTDOWN_MENU_APPLICATION_ID MATCHES "^[A-Za-z][A-Za-z0-9_-]*(\\.[A-Za-z][A-Za-z0-9_-]*)+$")
  message(FATAL_ERROR
    "SHUTDOWN_MENU_APPLICATION_ID must be a valid reverse-DNS application id: "
    "${SHUTDOWN_MENU_APPLICATION_ID}"
  )
endif()
string(REPLACE "." "/" _application_id_path "${SHUTDOWN_MENU_APPLICATION_ID}")
set(_application_id_path "/${_application_id_path}/")
set(_application_id "${SHUTDOWN_MENU_APPLICATION_ID}")

set(_stylesheet_install_dir "${CMAKE_INSTALL_DATADIR}/shutdown-menu")
set(_shutdown_command_location "${CMAKE_INSTALL_FULL_BINDIR}/shutdown-menu")

# include
find_package(PkgConfig 0.29 REQUIRED)
pkg_check_modules(
  GTKMM
  IMPORTED_TARGET
    # libadwaita-1
    gtk4
    gobject-2.0
  REQUIRED
)

# build
add_executable(shutdown-menu)
target_include_directories(shutdown-menu
  PRIVATE
    src
    ${GTKMM_INCLUDE_DIRS}
)
target_link_libraries(shutdown-menu
  PRIVATE
    ${GTKMM_LIBRARIES}
)
target_compile_definitions(shutdown-menu
  PUBLIC
    SHUTDOWN_MENU_STYLESHEET_DIR="$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>$<INSTALL_INTERFACE:${_stylesheet_install_dir}>/"
    SHUTDOWN_MENU_APPLICATION_ID="${_application_id}"
    _GNU_SOURCE
)
add_subdirectory(src)

# configure
configure_file(
  cmake/shutdown-menu-keybind.py.in
  ${CMAKE_SOURCE_DIR}/shutdown-menu-keybind
  @ONLY
)
# Keep a build-tree copy for tests and developers who do not want the
# generated script in the source tree.
configure_file(
  cmake/shutdown-menu-keybind.py.in
  ${CMAKE_BINARY_DIR}/shutdown-menu-keybind
  @ONLY
)

# GSettings schema
find_program(GLIB_COMPILE_SCHEMAS_EXECUTABLE NAMES glib-compile-schemas)
if(GLIB_COMPILE_SCHEMAS_EXECUTABLE)
  set(SHUTDOWN_MENU_SCHEMA_DIR "${CMAKE_BINARY_DIR}/schemas")
  set(SHUTDOWN_MENU_SCHEMA_FILE
    "${SHUTDOWN_MENU_SCHEMA_DIR}/shutdown-menu.gschema.xml"
  )
  file(MAKE_DIRECTORY "${SHUTDOWN_MENU_SCHEMA_DIR}")
  configure_file(
    cmake/shutdown-menu.gschema.xml.in
    "${SHUTDOWN_MENU_SCHEMA_FILE}"
    @ONLY
  )
  add_custom_command(
    OUTPUT "${SHUTDOWN_MENU_SCHEMA_DIR}/gschemas.compiled"
    COMMAND ${GLIB_COMPILE_SCHEMAS_EXECUTABLE} --strict "${SHUTDOWN_MENU_SCHEMA_DIR}"
    DEPENDS "${SHUTDOWN_MENU_SCHEMA_FILE}"
    COMMENT "Compiling GSettings schemas"
    VERBATIM
  )
  add_custom_target(shutdown-menu-schemas ALL
    DEPENDS "${SHUTDOWN_MENU_SCHEMA_DIR}/gschemas.compiled"
  )
else()
  message(WARNING
    "glib-compile-schemas not found; the GSettings schema will not be built. "
    "Install libglib2.0-bin to enable it."
  )
endif()

# tests
if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# install
install(
  TARGETS shutdown-menu
  CONFIGURATIONS Release RelWithDebInfo MinSizeRel
  RUNTIME
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
  FILES icon.png
  CONFIGURATIONS Release RelWithDebInfo MinSizeRel
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps/
  RENAME shutdown-menu.png
)
install(
  FILES stylesheet.css
  CONFIGURATIONS Release RelWithDebInfo MinSizeRel
  DESTINATION ${_stylesheet_install_dir}
)
# install(
#   CODE
#   "if (DEFINED ENV{PKEXEC_UID})
#     set(UID \$ENV{PKEXEC_UID})
#   elseif(DEFINED ENV{SUDO_UID})
#     set(UID \$ENV{SUDO_UID})
#   else()
#     message(FATAL_ERROR \"Unable to determine callee UID.\")
#   endif()
#   execute_process(
#     COMMAND sudo -H -u \"#\${UID}\" env DISPLAY=$ENV{DISPLAY} /usr/bin/python3 \"${CMAKE_SOURCE_DIR}/tools/enable-shutdown-menu-keybind.py\" --command \"${CMAKE_INSTALL_FULL_BINDIR}/shutdown-menu\"
#     RESULT_VARIABLE _ENABLE_SHUTDOWN_MENU_KEYBIND_RESULT
#   )
#   if (_ENABLE_SHUTDOWN_MENU_KEYBIND_RESULT GREATER 0)
#     message(FATAL_ERROR \"Enabling shutdown-menu keybinds failed with exit code \${_ENABLE_SHUTDOWN_MENU_KEYBIND_RESULT}\")
#   endif()"
#   CONFIGURATIONS Release RelWithDebInfo MinSizeRel
# )

# uninstall
# from: https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
if(NOT TARGET uninstall)
  configure_file(
    ${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in
    ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
    IMMEDIATE @ONLY
  )

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
  )
endif()
