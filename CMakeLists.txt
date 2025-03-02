## / ##

# required setup
cmake_minimum_required(VERSION 3.16)
project(shutdown-menu
  VERSION 0.1.1
  LANGUAGES C
)

# variables
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED True)

include(GNUInstallDirs)

set(_stylesheet_install_dir "${CMAKE_INSTALL_DATADIR}/shutdown-menu")
set(_shutdown_command_location "${CMAKE_INSTALL_FULL_BINDIR}/shutdown_menu")

option(DRY_RUN "Wether to compile shutdown-menu steril, meaning without effect." OFF)
if(${DRY_RUN})
  set(_dry_run_definition "DRY_RUN=\"--dry-run\", ")
else()
  set(_dry_run_definition "DRY_RUN=")
endif()

# include
find_package(PkgConfig 0.29 REQUIRED)
pkg_check_modules(
  GTKMM
  IMPORTED_TARGET
    # libadwaita-1
    gtk4
    gtk4-x11
    gobject-2.0
  REQUIRED
)

# build
add_executable(shutdown_menu)
target_include_directories(shutdown_menu
  PRIVATE
    src
    ${GTKMM_INCLUDE_DIRS}
)
target_link_libraries(shutdown_menu
  PRIVATE
    ${GTKMM_LIBRARIES}
)
target_compile_definitions(shutdown_menu
  PUBLIC
    ${_dry_run_definition}
    STYLESHEET_DIR="$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>$<INSTALL_INTERFACE:${_stylesheet_install_dir}>/"
    _GNU_SOURCE
)
add_subdirectory(src)

configure_file(
  cmake/shutdown-menu-keybind.py.in
  ${CMAKE_SOURCE_DIR}/shutdown-menu-keybind
  @ONLY
)

# install
install(
  TARGETS shutdown_menu
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
#     COMMAND sudo -H -u \"#\${UID}\" env DISPLAY=$ENV{DISPLAY} /usr/bin/python3 \"${CMAKE_SOURCE_DIR}/tools/enable-shutdown-menu-keybind.py\" --command \"${CMAKE_INSTALL_FULL_BINDIR}/shutdown_menu\"
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
