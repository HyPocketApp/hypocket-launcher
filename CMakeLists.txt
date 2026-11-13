cmake_minimum_required(VERSION 3.22)
project(mossling VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

option(MOSSLING_GAME "Build the game (needs raylib, downloaded automatically)" ON)
option(MOSSLING_TESTS "Build the tests of the game rules" ON)

# The game rules: plain C, no raylib, so they can be tested anywhere.
add_library(mossling_rules STATIC src/game.c src/levels.c)
target_include_directories(mossling_rules PUBLIC src)
if(NOT MSVC)
  target_compile_options(mossling_rules PRIVATE -Wall -Wextra)
  target_link_libraries(mossling_rules PUBLIC m)
endif()

if(MOSSLING_GAME)
  # raylib (zlib license). CMake downloads this exact release the first time.
  # To build offline, point FETCHCONTENT_SOURCE_DIR_RAYLIB at a local copy.
  include(FetchContent)
  set(RAYLIB_VERSION 6.2)
  if(ANDROID)
    set(PLATFORM "Android" CACHE STRING "raylib platform" FORCE)
  endif()
  set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(raylib
    URL https://github.com/raysan5/raylib/archive/refs/tags/${RAYLIB_VERSION}.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
  FetchContent_MakeAvailable(raylib)

  set(GAME_SOURCES src/main.c src/text.c)
  if(ANDROID)
    # Loaded by Android's NativeActivity; raylib's glue calls our main().
    add_library(mossling SHARED ${GAME_SOURCES})
    target_compile_definitions(mossling PRIVATE PLATFORM_ANDROID)
    target_include_directories(mossling PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
    target_link_options(mossling PRIVATE -u ANativeActivity_onCreate)
    target_link_libraries(mossling PRIVATE mossling_rules raylib android log EGL GLESv2 OpenSLES)
  else()
    add_executable(mossling ${GAME_SOURCES})
    target_link_libraries(mossling PRIVATE mossling_rules raylib)
    if(WIN32)
      set_target_properties(mossling PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
      if(NOT MSVC)
        target_link_options(mossling PRIVATE $<$<CONFIG:Release>:-mwindows>)
      endif()
    endif()
    if(APPLE)
      target_link_libraries(mossling PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
    endif()
  endif()
  if(NOT MSVC)
    target_compile_options(mossling PRIVATE -Wall -Wextra)
  endif()
endif()

if(MOSSLING_TESTS AND NOT ANDROID)
  enable_testing()
  add_executable(test_game tests/test_game.c)
  target_link_libraries(test_game PRIVATE mossling_rules)
  add_test(NAME game_rules COMMAND test_game)
endif()
