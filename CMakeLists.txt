cmake_minimum_required(VERSION 3.0)

set(BUILD_SHARED_LIBS ON)
add_subdirectory(libgit2-0.19.0)
include_directories(${CMAKE_SOURCE_DIR}/libgit2-0.19.0/include)
project(kup)

find_package(ECM REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR ${CMAKE_BUILD_TYPE} STREQUAL "DebugFull")
  message(WARNING "enabling debug output!")
  add_definitions(-DDEBUG)
else()
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

add_definitions(-DQT_NO_URL_CAST_FROM_STRING)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings)
include(FeatureSummary)
include(ECMInstallIcons)

find_package(Qt5 REQUIRED COMPONENTS Widgets)
find_package(KF5 REQUIRED COMPONENTS
Solid
KIO
IdleTime
I18n
Notifications
CoreAddons
IconThemes
DBusAddons
Config
Init # needed for the kdeinit cmake macro
JobWidgets
)

add_subdirectory(daemon)
add_subdirectory(icons)
add_subdirectory(filedigger)
add_subdirectory(kcm)
add_subdirectory(kioslave)
add_subdirectory(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

