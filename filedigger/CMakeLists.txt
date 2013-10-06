include_directories(${KDE4_INCLUDES})

set(filedigger_SRCS
filedigger.cpp
main.cpp
mergedvfs.cpp
mergedvfsmodel.cpp
restoredialog.cpp
restorejob.cpp
versionlistdelegate.cpp
versionlistmodel.cpp
../kioslave/vfshelpers.cpp
)

add_definitions(-fexceptions)

kde4_add_ui_files(filedigger_SRCS restoredialog.ui)
kde4_add_executable(kup-filedigger ${filedigger_SRCS})
target_link_libraries(kup-filedigger
QtCore
QtGui
kdecore
kdeui
kfile
kio
git24kup
)

########### install files ###############
install(TARGETS kup-filedigger ${INSTALL_TARGETS_DEFAULT_ARGS})
