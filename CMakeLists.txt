cmake_minimum_required(VERSION 3.28...3.30)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp
          src/hud-mask.cpp
          src/hud-mask.hpp
          src/cutout-editor.cpp
          src/cutout-editor.hpp
          src/mask-process.cpp
          src/mask-process.hpp
          src/presence.cpp
          src/presence.hpp
          src/update-check.cpp
          src/update-check.hpp
          src/update-parse.hpp
)

string(REGEX REPLACE "^https://github.com/" "" _gh_repo "${PLUGIN_WEBSITE}")
target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HUD_MASK_GH_REPO="${_gh_repo}")

if(OS_WINDOWS)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE winhttp)
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})

option(ENABLE_TESTS "Build unit tests" ON)
if(ENABLE_TESTS)
  enable_testing()
  add_executable(mask-process-tests tests/mask-process-test.cpp src/mask-process.cpp)
  target_include_directories(mask-process-tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
  add_test(NAME mask-process COMMAND mask-process-tests)
endif()
