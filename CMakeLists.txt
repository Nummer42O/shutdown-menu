## / ##

# required setup
cmake_minimum_required(VERSION 3.16)
project(shutdown-menu
  LANGUAGES C
)

# variables
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(GNUInstallDirs)

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

add_subdirectory(src)

# install
install(
  TARGETS shutdown_menu
  # RUNTIME
  #   DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# uninstall
# from: https://gitlab.kitware.com/cmake/community/-/wikis/FAQ#can-i-do-make-uninstall-with-cmake
if(NOT TARGET uninstall)
  configure_file(
    ${CMAKE_SOURCE_DIR}/tools/cmake/cmake_uninstall.cmake.in
    ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
    IMMEDIATE @ONLY
  )

  add_custom_target(uninstall
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_BINARY_DIR}/cmake_uninstall.cmake
  )
endif()
