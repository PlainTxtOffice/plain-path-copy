cmake_minimum_required(VERSION 3.25)

# The application version. Keep it in step with product_versions in
# docs/USER_MANUAL.md; the package manifest and file resources read it here.
project(plain-path-copy VERSION 1.0.0 LANGUAGES CXX)

if(NOT MSVC)
  message(FATAL_ERROR "Plain Path Copy builds only with MSVC.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
# Link the C runtime statically so the portable download needs no VC++
# redistributable. Explorer loads the DLL into dllhost.exe on any machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(APP_NAME "Plain Path Copy")
set(APP_SLUG "plain-path-copy")
set(APP_MENU_TITLE "Copy as path (/)")
set(APP_PACKAGE_NAME "PlainTextOffice.PlainPathCopy")
set(APP_PUBLISHER
  "CN=Plain Text Office LLC, OU=Software, O=Plain Text Office LLC, L=Spokane, S=Washington, C=US")
set(APP_PUBLISHER_DISPLAY_NAME "Plain Text Office")
set(APP_COMMAND_CLSID "9CC2DAEC-BEB6-4717-AB90-CF336A7A405A")
set(APP_SHELL_DLL "${APP_SLUG}-shell.dll")
set(APP_EXE "${APP_SLUG}.exe")
set(APP_PACKAGE_FILE "${APP_SLUG}.msix")
set(APP_VERSION_QUAD "${PROJECT_VERSION}.0")
string(REPLACE "." "," APP_VERSION_COMMAS "${APP_VERSION_QUAD}")

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
configure_file(src/core/app-info.h.in "${GENERATED_DIR}/app-info.h" @ONLY)

# Executables, the DLL, and the package files share one folder per
# configuration, build/x64/layout/<Config>/, which is the external location
# the development package points at.
set(LAYOUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/layout/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${LAYOUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${LAYOUT_DIR}")

add_compile_definitions(
  UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
  _WIN32_WINNT=0x0A00 NTDDI_VERSION=0x0A00000B)
add_compile_options(/W4 /WX /permissive- /utf-8 /EHsc /Zc:__cplusplus)

# Path formatting, settings, the clipboard, and feedback drafts, shared by
# every target.
add_library(path-copy-core STATIC
  src/core/clipboard.cpp
  src/core/feedback.cpp
  src/core/path-format.cpp
  src/core/settings.cpp)
target_include_directories(path-copy-core PUBLIC src/core "${GENERATED_DIR}")
target_link_libraries(path-copy-core PUBLIC advapi32 user32 shell32 ntdll)

# The File Explorer command, loaded by the sparse package's COM surrogate.
add_library(path-copy-shell SHARED
  src/shell/copy-path-command.cpp
  src/shell/shell-extension.def
  src/shell/shell-extension.rc)
set_target_properties(path-copy-shell PROPERTIES OUTPUT_NAME "${APP_SLUG}-shell")
target_include_directories(path-copy-shell PRIVATE src/shell assets)
target_link_libraries(path-copy-shell PRIVATE path-copy-core shell32 shlwapi
  ole32 runtimeobject)

# The settings and feedback window, which also adds and removes the package.
configure_file(src/app/app.manifest.in "${GENERATED_DIR}/app.manifest" @ONLY)
add_executable(path-copy-app WIN32
  src/app/main.cpp
  src/app/package-registration.cpp
  src/app/settings-window.cpp
  src/app/app.rc)
set_target_properties(path-copy-app PROPERTIES OUTPUT_NAME "${APP_SLUG}")
target_include_directories(path-copy-app PRIVATE src/app assets)
target_link_libraries(path-copy-app PRIVATE path-copy-core comctl32 shlwapi
  ole32 windowsapp)
target_link_options(path-copy-app PRIVATE /MANIFEST:NO)

# The sparse package manifest and its logos, copied beside the DLL.
configure_file(package/AppxManifest.xml.in
  "${CMAKE_CURRENT_BINARY_DIR}/package/AppxManifest.xml" @ONLY)
file(GLOB PACKAGE_LOGOS CONFIGURE_DEPENDS assets/package/*.png)
# A target rather than a post-build step, so a manifest change is copied
# even when the DLL does not relink.
add_custom_target(package-layout ALL
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${LAYOUT_DIR}/assets"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different
    "${CMAKE_CURRENT_BINARY_DIR}/package/AppxManifest.xml" "${LAYOUT_DIR}"
  COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${PACKAGE_LOGOS}
    "${LAYOUT_DIR}/assets"
  DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/package/AppxManifest.xml"
    ${PACKAGE_LOGOS}
  VERBATIM)

# Rasterizes the master SVG icon; run by scripts/make-icons.ps1.
add_executable(make-icons tools/make-icons/make-icons.cpp)
set_target_properties(make-icons PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tools/$<CONFIG>")
target_link_libraries(make-icons PRIVATE d2d1 windowscodecs ole32 shlwapi)

enable_testing()
foreach(test_name IN ITEMS feedback-tests path-format-tests
    shell-extension-tests)
  add_executable(${test_name} tests/${test_name}.cpp)
  set_target_properties(${test_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/tests/$<CONFIG>")
  target_include_directories(${test_name} PRIVATE tests)
  target_link_libraries(${test_name} PRIVATE path-copy-core shell32 ole32)
  add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
target_compile_definitions(shell-extension-tests PRIVATE
  SHELL_DLL_PATH=L"$<TARGET_FILE:path-copy-shell>")
add_dependencies(shell-extension-tests path-copy-shell)
