cmake_minimum_required(VERSION 3.16...3.26)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" ON)
option(ENABLE_QT "Use Qt functionality" ON)

include(compilerconfig)
include(defaults)
include(helpers)

set(RESOURCE_FILES
    resources/resources.qrc
    resources/images/trash.svg
    resources/images/popout.svg
)

add_library(${CMAKE_PROJECT_NAME} MODULE ${RESOURCE_FILES})

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 REQUIRED COMPONENTS Widgets Core Gui Svg)
  
  if(Qt6_FOUND)
    target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE -DENABLE_QT)
    target_compile_options(
      ${CMAKE_PROJECT_NAME} PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header
                                    -Wno-comma>)
    set_target_properties(
      ${CMAKE_PROJECT_NAME}
      PROPERTIES AUTOMOC ON
                 AUTOUIC ON
                 AUTORCC ON)
                 
    qt_add_resources(RESOURCE_FILES /resources/resources.qrc)

  else()
    message(FATAL_ERROR "Qt6 not found. Please install Qt6 and try again.")
  endif()
endif()

target_sources(${CMAKE_PROJECT_NAME}
  PRIVATE src/plugin-main.cpp
  PRIVATE src/window-dock-ui.cpp
)

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})