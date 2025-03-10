cmake_minimum_required(VERSION 3.16)

## Add paths to check for cmake modules:
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

option(BUILD_WITH_PCH "Build with precompiled headers" ON)

include(FeatureSummary)
include(QtCreatorIDEBranding RESULT_VARIABLE IDE_BRANDING_FILE)
include(QtCreatorTranslations)
include(QtCreatorDocumentation)
include(QtCreatorAPI)

set(IDE_REVISION FALSE CACHE BOOL "Marks the presence of IDE revision string.")
set(IDE_REVISION_STR "" CACHE STRING "The IDE revision string.")
set(IDE_REVISION_URL "" CACHE STRING "The IDE revision Url string.")

mark_as_advanced(IDE_REVISION IDE_REVISION_STR IDE_REVISION_URL)

project(QtCreator VERSION ${IDE_VERSION})

# Force C++ standard, do not fall back, do not use compiler extensions
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

qtc_handle_compiler_cache_support()

option(BUILD_LINK_WITH_QT "Link with Qt from the parent Qt Creator" OFF)
qtc_link_with_qt()

option(WITH_TESTS "Build Tests" OFF)
add_feature_info("Build tests" ${WITH_TESTS} "")
option(WITH_DEBUG_CMAKE "Enabled CMake project debugging functionality" OFF)
option(SHOW_BUILD_DATE "Show build date in about dialog" OFF)
option(WITH_SANITIZE "Build with sanitizer enabled" OFF)
set(SANITIZE_FLAGS "" CACHE STRING "Sets flags for sanitizer compilation flags used in Debug builds")
add_feature_info("Build with sanitize" ${WITH_SANITIZE} "SANITIZE_FLAGS='${SANITIZE_FLAGS}'")


# merge binary directories of sub projects into top level
set(QTC_MERGE_BINARY_DIR ON)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Set up Qt stuff:
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if (WITH_TESTS)
  set(QT_TEST_COMPONENT Test)
  set(IMPLICIT_DEPENDS Qt5::Test)
endif()

find_package(Qt5
  ${IDE_QT_VERSION_MIN}
  COMPONENTS Concurrent Core Gui Network PrintSupport Qml Sql Widgets Xml Core5Compat ${QT_TEST_COMPONENT}
  REQUIRED
)
if (Qt5_VERSION VERSION_LESS 6.0.0)
  install(TARGETS Qt6Core5Compat EXPORT QtCreator)

  # Specify standards conformance mode to MSVC 2017 and later
  # Qt6 has the values as part of the Qt6::Platform target interface
  if (MSVC AND MSVC_VERSION GREATER_EQUAL 1910)
    add_compile_options(/permissive- /Zc:__cplusplus)
  endif()
  if (CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    add_compile_options(-Wno-missing-field-initializers)
  endif()
else()
  # Common intermediate directory for QML modules which are defined via qt_add_qml_module()
  set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml_modules")

  # This includes the code that will enable higher compiler warnings level (/W3 for MSVC, -Wall -Wextra for GCC)
  # This is controlled by QT_COMPILE_OPTIONS_DISABLE_WARNINGS target property.
  include(QtCompilerFlags)

  if (MSVC AND QT_FEATURE_static_runtime)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
  endif()
endif()
find_package(Qt5 COMPONENTS LinguistTools QUIET)
find_package(Qt5 COMPONENTS Quick QuickWidgets Designer DesignerComponents Help SerialPort Svg Tools QUIET)

find_package(Threads)
find_package(Clang QUIET)
if (Qt5_VERSION VERSION_LESS 6.0.0)
  find_package(KF5SyntaxHighlighting QUIET)
endif()

# Crashpad
set(CRASHPAD_BACKEND_URL "" CACHE STRING "Crashpad backend URL")
set(BUILD_WITH_CRASHPAD OFF)
# Linux is not supported for now
# x86_64;arm64 is not supported for now
if(CRASHPAD_BACKEND_URL AND (WIN32 OR (APPLE AND NOT "${CMAKE_OSX_ARCHITECTURES}" STREQUAL "x86_64;arm64")))
  find_package(Crashpad QUIET)
  if(TARGET Crashpad::Crashpad)
    set(BUILD_WITH_CRASHPAD ON)
  endif()
endif()
add_feature_info("Build with Crashpad" ${BUILD_WITH_CRASHPAD} "")

function (set_if_target var target)
   if (TARGET "${target}")
     set(_result ON)
   else()
     set(_result OFF)
   endif()
   set(${var} "${_result}" PARENT_SCOPE)
endfunction()

set_if_target(_has_svg_target Qt5::Svg)
option(ENABLE_SVG_SUPPORT "Enable SVG support" "${_has_svg_target}")

add_library(OptionalSvg INTERFACE)
if (TARGET Qt5::Svg AND ENABLE_SVG_SUPPORT)
  target_link_libraries(OptionalSvg INTERFACE Qt5::Svg)
else()
  target_compile_definitions(OptionalSvg INTERFACE QT_NO_SVG)
endif()
install(TARGETS OptionalSvg EXPORT QtCreator)

if (APPLE)
  find_library(FWCoreFoundation CoreFoundation)
  find_library(FWCoreServices CoreServices)
  find_library(FWFoundation Foundation)
  find_library(FWAppKit AppKit)
  find_library(FWIOKit IOKit)
  find_library(FWSecurity Security)
  find_library(FWSystemConfiguration SystemConfiguration)
  find_library(FWWebKit WebKit)
endif()

if (WITH_TESTS)
  enable_testing()
endif()

if (UNIX)
  add_subdirectory(bin)
endif()

add_subdirectory(src)
add_subdirectory(share)

if (WITH_TESTS)
  add_subdirectory(tests)
endif()

add_subdirectory(doc)

find_package(Python3 COMPONENTS Interpreter)
if (NOT Python3_Interpreter_FOUND)
  message("No python interpreter found, skipping \"Dependencies\" install component.")
else()
  get_target_property(_qmake_binary Qt5::qmake IMPORTED_LOCATION)
  set(_llvm_arg)
  if (LLVM_INSTALL_PREFIX)
    set(_llvm_arg "--llvm \"${LLVM_INSTALL_PREFIX}\"")
  endif()
  set(_elfutils_arg)
  if (ELFUTILS_INCLUDE_DIR)
    get_filename_component(_elfutils_path ${ELFUTILS_INCLUDE_DIR} DIRECTORY)
    set(_elfutils_arg "--elfutils \"${_elfutils_path}\"")
  endif()
  install(CODE "
      execute_process(COMMAND
        \"${Python3_EXECUTABLE}\"
        \"${CMAKE_CURRENT_LIST_DIR}/scripts/deployqt.py\"
        ${_llvm_arg}
        ${_elfutils_arg}
        \"\${CMAKE_INSTALL_PREFIX}/${IDE_APP_PATH}/${IDE_APP_TARGET}\"
        \"${_qmake_binary}\"
        COMMAND_ECHO STDOUT
        )
    "
    COMPONENT Dependencies
    EXCLUDE_FROM_ALL
  )
endif()

feature_summary(INCLUDE_QUIET_PACKAGES WHAT
  PACKAGES_FOUND PACKAGES_NOT_FOUND
  ENABLED_FEATURES DISABLED_FEATURES
)
