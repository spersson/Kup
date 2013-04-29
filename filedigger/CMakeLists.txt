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

kde4_add_executable(kup_filedigger ${filedigger_SRCS})
target_link_libraries(kup_filedigger
QtCore
QtGui
kdecore
kdeui
kio
git2
)

########### install files ###############
install(TARGETS kup_filedigger ${INSTALL_TARGETS_DEFAULT_ARGS})
