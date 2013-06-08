project(kup)

find_package(KDE4 REQUIRED)
include (KDE4Defaults)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR ${CMAKE_BUILD_TYPE} STREQUAL "DebugFull")
  message(WARNING "enabling debug output!")
  add_definitions(-DDEBUG)
else()
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

add_definitions(-DQT_NO_CAST_FROM_ASCII)


add_subdirectory(daemon)
add_subdirectory(icons)
add_subdirectory(filedigger)
add_subdirectory(kcm)
add_subdirectory(kioslave)
add_subdirectory(po)
