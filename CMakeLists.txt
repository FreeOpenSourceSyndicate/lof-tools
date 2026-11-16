cmake_minimum_required(VERSION 3.16)
project(lof-tools LANGUAGES CXX VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

# ---------------------------------------------------------------------------
# Core library: file models + cheat-protection logic.  This is the reusable
# part the future save-game and character editors build on.
# ---------------------------------------------------------------------------
add_library(lofcore STATIC
  src/core/file_io.cpp
  src/core/checksum.cpp
  src/core/tables/items.cpp
  src/core/tables/spells.cpp
  src/core/names.cpp
  src/core/character.cpp
  src/core/rost.cpp
  src/core/savegame.cpp
  src/core/document.cpp
  src/core/sanitizer.cpp
)
target_include_directories(lofcore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(MSVC)
  target_compile_options(lofcore PRIVATE /W3)
else()
  target_compile_options(lofcore PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------------------
# CLI: headless sanitizer (also useful for scripting and for verifying the
# GUI's results).
# ---------------------------------------------------------------------------
add_executable(lof-cli src/cli/main.cpp)
target_link_libraries(lof-cli PRIVATE lofcore)

# ---------------------------------------------------------------------------
# GUI: Qt6 Widgets front-end.  Optional - the core library and CLI build even
# when Qt6 is not installed.
# ---------------------------------------------------------------------------
find_package(Qt6 COMPONENTS Widgets QUIET)
if(Qt6_FOUND)
  set(CMAKE_AUTOMOC ON)
  add_executable(lof-tools
    src/gui/main.cpp
    src/gui/main_window.cpp
    src/gui/sanitizer_dialog.cpp
    src/gui/character_editor_dialog.cpp
  )
  target_link_libraries(lof-tools PRIVATE lofcore Qt6::Widgets)
  if(MSVC)
    set_target_properties(lof-tools PROPERTIES WIN32_EXECUTABLE OFF)
  endif()
  message(STATUS "Qt6 Widgets found - building the GUI (lof-tools)")
else()
  message(STATUS "Qt6 Widgets NOT found - building the CLI only (lof-cli)")
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()
add_executable(lof-tests tests/test_core.cpp)
target_link_libraries(lof-tests PRIVATE lofcore)
add_test(NAME core COMMAND lof-tests ${CMAKE_CURRENT_SOURCE_DIR}/testdata)
set_tests_properties(core PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
