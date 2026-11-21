## /tests/ ##

find_package(Python3 COMPONENTS Interpreter QUIET)
find_program(_bash_executable NAMES bash REQUIRED)
find_program(_gsettings_executable NAMES gsettings)
find_program(_xephyr_executable NAMES Xephyr)
find_program(_gnome_shell_executable NAMES gnome-shell)
find_program(_import_executable NAMES import)
find_program(_compare_executable NAMES compare)
find_program(_convert_executable NAMES convert)
find_program(_dbus_run_session_executable NAMES dbus-run-session)
find_program(_gdbus_executable NAMES gdbus)

set(_shutdown_menu_test_artifacts "${CMAKE_BINARY_DIR}/test-artifacts")
file(MAKE_DIRECTORY "${_shutdown_menu_test_artifacts}")

set(_shutdown_menu_test_schema_dir "")
if(EXISTS "${_shutdown_menu_schema_file}")
  set(_shutdown_menu_test_schema_dir "${_shutdown_menu_schema_dir}")
endif()

# --- unit tests -----------------------------------------------------------

if(_glib_compile_schemas AND EXISTS "${_shutdown_menu_schema_file}")
  add_test(
    NAME unit.schema
    COMMAND "${_bash_executable}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/schema.sh"
      "${_shutdown_menu_schema_dir}"
      "${_glib_compile_schemas}"
  )
  set_tests_properties(unit.schema PROPERTIES
    LABELS "unit"
    TIMEOUT 30
  )
endif()

if(Python3_Interpreter_FOUND AND _gsettings_executable)
  add_test(
    NAME unit.keybind
    COMMAND "${_bash_executable}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/keybind.sh"
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
  COMMAND "${_bash_executable}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/cli.sh"
    "$<TARGET_FILE:shutdown-menu>"
)
set_tests_properties(unit.cli PROPERTIES
  LABELS "unit"
  TIMEOUT 30
)

if(Python3_Interpreter_FOUND AND _dbus_run_session_executable AND _gdbus_executable)
  add_test(
    NAME unit.mock-logind
    COMMAND "${_bash_executable}" "${CMAKE_CURRENT_SOURCE_DIR}/unit/mock-logind.sh"
      "${Python3_EXECUTABLE}"
      "${_gdbus_executable}"
      "${_shutdown_menu_test_artifacts}/mock-logind"
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
  COMMAND "${_bash_executable}" "${CMAKE_CURRENT_SOURCE_DIR}/integration/wayland-smoke.sh"
    "$<TARGET_FILE:shutdown-menu>"
    "${_shutdown_menu_test_artifacts}/wayland"
    "${_shutdown_menu_test_schema_dir}"
)
set_tests_properties(integration.wayland-smoke PROPERTIES
  LABELS "integration"
  TIMEOUT 180
  SKIP_RETURN_CODE 77
)

add_test(
  NAME integration.x11-smoke
  COMMAND "${_bash_executable}" "${CMAKE_CURRENT_SOURCE_DIR}/integration/x11-smoke.sh"
    "$<TARGET_FILE:shutdown-menu>"
    "${_shutdown_menu_test_artifacts}/x11"
    "${_shutdown_menu_test_schema_dir}"
)
set_tests_properties(integration.x11-smoke PROPERTIES
  LABELS "integration"
  TIMEOUT 120
  SKIP_RETURN_CODE 77
)

# --- XTest helper ---------------------------------------------------------

pkg_check_modules(_x11_xtest QUIET IMPORTED_TARGET xtst x11)
if(_x11_xtest_FOUND)
  add_executable(shutdown-menu-test-click helpers/xtest-click.c)
  target_link_libraries(shutdown-menu-test-click PRIVATE PkgConfig::_x11_xtest)
endif()
