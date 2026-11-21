## /tests/ ##

find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(BASH_EXECUTABLE NAMES bash REQUIRED)
find_program(GSETTINGS_EXECUTABLE NAMES gsettings)
find_program(XEPHYR_EXECUTABLE NAMES Xephyr)
find_program(GNOME_SHELL_EXECUTABLE NAMES gnome-shell)
find_program(IMPORT_EXECUTABLE NAMES import)
find_program(COMPARE_EXECUTABLE NAMES compare)
find_program(CONVERT_EXECUTABLE NAMES convert)
find_program(DBUS_RUN_SESSION_EXECUTABLE NAMES dbus-run-session)
find_program(GDBUS_EXECUTABLE NAMES gdbus)

set(SHUTDOWN_MENU_TEST_ARTIFACTS "${CMAKE_BINARY_DIR}/test-artifacts")
file(MAKE_DIRECTORY "${SHUTDOWN_MENU_TEST_ARTIFACTS}")

# --- unit tests -----------------------------------------------------------

if(GLIB_COMPILE_SCHEMAS_EXECUTABLE AND EXISTS "${SHUTDOWN_MENU_SCHEMA_FILE}")
  add_test(
    NAME unit.schema
    COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/schema.sh"
      "${SHUTDOWN_MENU_SCHEMA_DIR}"
      "${GLIB_COMPILE_SCHEMAS_EXECUTABLE}"
  )
  set_tests_properties(unit.schema PROPERTIES
    LABELS "unit"
    TIMEOUT 30
  )
endif()

if(Python3_Interpreter_FOUND AND GSETTINGS_EXECUTABLE)
  add_test(
    NAME unit.keybind
    COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/keybind.sh"
      "${Python3_EXECUTABLE}"
      "${CMAKE_BINARY_DIR}/shutdown-menu-keybind"
      "$<TARGET_FILE:shutdown-menu>"
  )
  set_tests_properties(unit.keybind PROPERTIES
    LABELS "unit"
    TIMEOUT 60
    SKIP_RETURN_CODE 77
  )
endif()

add_test(
  NAME unit.cli
  COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/cli.sh"
    "$<TARGET_FILE:shutdown-menu>"
)
set_tests_properties(unit.cli PROPERTIES
  LABELS "unit"
  TIMEOUT 30
)

if(Python3_Interpreter_FOUND AND DBUS_RUN_SESSION_EXECUTABLE AND GDBUS_EXECUTABLE)
  add_test(
    NAME unit.mock-logind
    COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/mock-logind.sh"
      "${Python3_EXECUTABLE}"
      "${GDBUS_EXECUTABLE}"
      "${SHUTDOWN_MENU_TEST_ARTIFACTS}/mock-logind"
  )
  set_tests_properties(unit.mock-logind PROPERTIES
    LABELS "unit"
    TIMEOUT 60
    SKIP_RETURN_CODE 77
  )
endif()

# --- integration tests ----------------------------------------------------

add_test(
  NAME integration.wayland-smoke
  COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/integration/wayland-smoke.sh"
    "$<TARGET_FILE:shutdown-menu>"
    "${SHUTDOWN_MENU_TEST_ARTIFACTS}/wayland"
    "${SHUTDOWN_MENU_SCHEMA_DIR}"
)
set_tests_properties(integration.wayland-smoke PROPERTIES
  LABELS "integration"
  TIMEOUT 180
  SKIP_RETURN_CODE 77
)

add_test(
  NAME integration.x11-smoke
  COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/integration/x11-smoke.sh"
    "$<TARGET_FILE:shutdown-menu>"
    "${SHUTDOWN_MENU_TEST_ARTIFACTS}/x11"
    "${SHUTDOWN_MENU_SCHEMA_DIR}"
)
set_tests_properties(integration.x11-smoke PROPERTIES
  LABELS "integration"
  TIMEOUT 120
  SKIP_RETURN_CODE 77
)

# --- XTest helper ---------------------------------------------------------

pkg_check_modules(X11_XTEST QUIET IMPORTED_TARGET xtst x11)
if(X11_XTEST_FOUND)
  add_executable(shutdown-menu-test-click helpers/xtest-click.c)
  target_link_libraries(shutdown-menu-test-click PRIVATE PkgConfig::X11_XTEST)
endif()
