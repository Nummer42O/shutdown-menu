## / ##

# setup

## standard CMake

cmake_minimum_required(VERSION 3.16)
project(shutdown-menu
  VERSION 0.2.1
  LANGUAGES C
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(GNUInstallDirs)
include(CTest)

## configurable variables

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

## derived variables

string(REPLACE "." "/" _application_id_path "${SHUTDOWN_MENU_APPLICATION_ID}")
set(_application_id_path "/${_application_id_path}/")
set(_application_id "${SHUTDOWN_MENU_APPLICATION_ID}")

set(_shutdown_command_location "${CMAKE_INSTALL_FULL_BINDIR}/shutdown-menu")
set(_shutdown_menu_keybind_helper "${CMAKE_BINARY_DIR}/shutdown-menu-keybind")

# Keep the schema paths defined even when glib-compile-schemas is unavailable;
# tests use them and skip schema-specific checks independently.
set(_shutdown_menu_schema_dir "${CMAKE_BINARY_DIR}/schemas")
set(_shutdown_menu_schema_file "${_shutdown_menu_schema_dir}/shutdown-menu.gschema.xml")

set(_gresource_xml_in_file "${CMAKE_SOURCE_DIR}/cmake/shutdown-menu.gresource.xml.in")
set(_gresource_xml_file "${CMAKE_BINARY_DIR}/shutdown-menu.gresource.xml")
set(_gresource_c_file "${CMAKE_BINARY_DIR}/generated/shutdown-menu.gresource.c")

## packages and host tools

find_package(PkgConfig 0.29 REQUIRED)
pkg_check_modules(
  _gtk4
  IMPORTED_TARGET
    gtk4
  REQUIRED
)
find_program(_glib_compile_resources
  NAMES glib-compile-resources
  REQUIRED
)
find_program(_glib_compile_schemas
  NAMES glib-compile-schemas
)

# generated files

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

## GResource

configure_file(
  "${_gresource_xml_in_file}"
  "${_gresource_xml_file}"
  @ONLY
)

execute_process(
  COMMAND "${_glib_compile_resources}"
    --generate-dependencies
    --sourcedir "${CMAKE_SOURCE_DIR}/resources"
    "${_gresource_xml_file}"
  OUTPUT_VARIABLE _gresource_dependencies
  RESULT_VARIABLE _gresource_dependencies_result
)
if(NOT _gresource_dependencies_result EQUAL 0)
  message(FATAL_ERROR "Unable to generate GResource dependencies for ${_gresource_xml_file}")
endif()
# convert newline separated string into valid CMake list
string(REPLACE "\n" ";" _gresource_dependencies "${_gresource_dependencies}")
# remove last empty element as it is an artifact due to trailing newline
list(POP_BACK _gresource_dependencies)

add_custom_command(
  OUTPUT "${_gresource_c_file}"
  COMMAND "${_glib_compile_resources}"
  ARGS
    --generate-source
    --sourcedir "${CMAKE_SOURCE_DIR}/resources"
    --target "${_gresource_c_file}"
    --c-name shutdown_menu_
    --internal
    "${_gresource_xml_file}"
  VERBATIM
  MAIN_DEPENDENCY "${_gresource_xml_file}"
  DEPENDS "${_gresource_dependencies}"
)
add_custom_target(shutdown-menu-gresource
  DEPENDS "${_gresource_c_file}"
)

## keybind helper

# This script is intentionally not installed; run it from the build tree.
configure_file(
  "${CMAKE_SOURCE_DIR}/cmake/shutdown-menu-keybind.py.in"
  "${_shutdown_menu_keybind_helper}"
  @ONLY
)

## GSettings schema

if(_glib_compile_schemas)
  file(MAKE_DIRECTORY "${_shutdown_menu_schema_dir}")
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/shutdown-menu.gschema.xml.in"
    "${_shutdown_menu_schema_file}"
    @ONLY
  )
  add_custom_command(
    OUTPUT "${_shutdown_menu_schema_dir}/gschemas.compiled"
    COMMAND "${_glib_compile_schemas}" --strict "${_shutdown_menu_schema_dir}"
    DEPENDS "${_shutdown_menu_schema_file}"
    COMMENT "Compiling GSettings schemas"
    VERBATIM
  )
  add_custom_target(shutdown-menu-schemas ALL
    DEPENDS "${_shutdown_menu_schema_dir}/gschemas.compiled"
  )
else()
  message(WARNING
    "glib-compile-schemas not found; the GSettings schema will not be built. "
    "Install libglib2.0-bin to enable it."
  )
endif()

# main target shutdown-menu

add_executable(shutdown-menu)
target_sources(shutdown-menu
  PRIVATE
    "${_gresource_c_file}"
)
add_subdirectory(src)
target_include_directories(shutdown-menu
  PRIVATE
    src
)
target_link_libraries(shutdown-menu
  PRIVATE
    PkgConfig::_gtk4
)
target_compile_definitions(shutdown-menu
  PRIVATE
    SHUTDOWN_MENU_APPLICATION_ID="${_application_id}"
    SHUTDOWN_MENU_APPLICATION_ID_PATH="${_application_id_path}"
    _GNU_SOURCE
)
add_dependencies(shutdown-menu shutdown-menu-gresource)

# tests

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()

# installation

## custom keybind helper target

# NOTE: If the script starts creating log files at some point, list them as byproducts.
add_custom_target(enable-keybinds
  COMMAND "${_shutdown_menu_keybind_helper}" enable
  DEPENDS "${_shutdown_menu_keybind_helper}"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Install/Enable keybinds for the shutdown menu."
  VERBATIM USES_TERMINAL
)
add_custom_target(force-enable-keybinds
  COMMAND "${_shutdown_menu_keybind_helper}" enable --problem-handler disable
  DEPENDS "${_shutdown_menu_keybind_helper}"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Install/Enable keybinds for the shutdown menu."
  VERBATIM USES_TERMINAL
)
add_custom_target(disable-keybinds
  COMMAND "${_shutdown_menu_keybind_helper}" disable
  DEPENDS "${_shutdown_menu_keybind_helper}"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Disable keybinds for the shutdown menu."
  VERBATIM USES_TERMINAL
)
add_custom_target(purge-keybinds
  COMMAND "${_shutdown_menu_keybind_helper}" disable --purge
  DEPENDS "${_shutdown_menu_keybind_helper}"
  WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
  COMMENT "Remove keybinds for the shutdown menu."
  VERBATIM USES_TERMINAL
)

## install

# NOTE: The generated keybind helper is intentionally not installed. Run it from the build tree instead.

install(
  TARGETS shutdown-menu
  RUNTIME
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
)
install(
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png"
  DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps/"
  RENAME shutdown-menu.png
)

## uninstall

# from: https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
if(NOT TARGET uninstall)
  configure_file(
    "${CMAKE_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY
  )

  add_custom_target(uninstall
    COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_BINARY_DIR}/cmake_uninstall.cmake"
  )
endif()
