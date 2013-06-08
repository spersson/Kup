include_directories(${KDE4_INCLUDES})

set(filedigger_SRCS
filedigger.cpp
main.cpp
mergedvfs.cpp
mergedvfsmodel.cpp
versionlistmodel.cpp
../kioslave/vfshelpers.cpp
)

add_definitions(-fexceptions)

kde4_add_executable(kup-filedigger ${filedigger_SRCS})
target_link_libraries(kup-filedigger
QtCore
QtGui
kdecore
kdeui
kio
git2
)

########### install files ###############
install(TARGETS kup-filedigger ${INSTALL_TARGETS_DEFAULT_ARGS})
