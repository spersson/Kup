project(kup)

add_definitions(-fPIC)

find_package(KDE4 REQUIRED)
include (KDE4Defaults)

if(${CMAKE_BUILD_TYPE} STREQUAL "Debug")
  message(WARNING "enabling debug output!")
  add_definitions(-DDEBUG)
else()
  add_definitions(-DQT_NO_DEBUG_OUTPUT)
endif()

add_subdirectory(settings)
add_subdirectory(kcm)
add_subdirectory(daemon)
#add_subdirectory(runner)
#add_subdirectory(kioslave)

