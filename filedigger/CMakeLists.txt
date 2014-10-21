include_directories(${KDE4_INCLUDES})
include_directories("../daemon")
include_directories("../kioslave")

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
${QT_QTCORE_LIBRARY}
${QT_QTGUI_LIBRARY}
${KDE4_KDECORE_LIBRARY}
${KDE4_KDEUI_LIBRARY}
${KDE4_KIO_LIBRARY}
${KDE4_KFILE_LIBRARY}
git24kup
)

########### install files ###############
install(TARGETS kup-filedigger ${INSTALL_TARGETS_DEFAULT_ARGS})
