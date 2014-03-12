
include_directories(${KDE4_INCLUDES})
include_directories("../settings")

set(kupdaemon_SRCS
main.cpp
kupdaemon.cpp
planexecutor.cpp
edexecutor.cpp
fsexecutor.cpp
backupjob.cpp
bupjob.cpp
rsyncjob.cpp
../settings/backupplan.cpp
../settings/kupsettings.cpp
)

kde4_add_kdeinit_executable(kup-daemon ${kupdaemon_SRCS})
target_link_libraries(kdeinit_kup-daemon
${QT_QTCORE_LIBRARY}
${QT_QTDBUS_LIBRARY}
${QT_QTGUI_LIBRARY}
${KDE4_KDECORE_LIBRARY}
${KDE4_KDEUI_LIBRARY}
${KDE4_KIO_LIBRARY}
${KDE4_KFILE_LIBRARY}
${KDE4_KIDLETIME_LIBRARY}
${KDE4_SOLID_LIBRARY}
# save some unnecessary library linking by stating the used libs directly instead of these:
#${KDE4_KDEUI_LIBS}
#${KDE4_KFILE_LIBS}
#${KDE4_SOLID_LIBS}
#${KDE4_KIDLETIME_LIBS}
)

########### install files ###############
install(TARGETS kup-daemon ${INSTALL_TARGETS_DEFAULT_ARGS})
install(TARGETS kdeinit_kup-daemon ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES kup-daemon.desktop DESTINATION ${AUTOSTART_INSTALL_DIR})
install(FILES kup-daemon.notifyrc DESTINATION ${DATA_INSTALL_DIR}/kup-daemon)

