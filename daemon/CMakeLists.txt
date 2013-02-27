
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
)

kde4_add_executable(kupdaemon ${kupdaemon_SRCS})
target_link_libraries(kupdaemon
kupsettings
${KDE4_KDEUI_LIBS}
${KDE4_KFILE_LIBS}
${KDE4_SOLID_LIBS}
${KDE4_KIDLETIME_LIBS}
)

kde4_add_executable(kuphelper kuphelper.cpp backupjob.cpp bupjob.cpp rsyncjob.cpp)
target_link_libraries(kuphelper ${KDE4_KDECORE_LIBS})

########### install files ###############
install(TARGETS kupdaemon ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES kupdaemon.desktop DESTINATION ${AUTOSTART_INSTALL_DIR})
install(FILES kupdaemon.notifyrc DESTINATION ${DATA_INSTALL_DIR}/kupdaemon)
install(TARGETS kuphelper DESTINATION ${LIBEXEC_INSTALL_DIR})
kde4_install_auth_actions(org.kde.kup.runner kuphelper.actions)
kde4_install_auth_helper_files(kuphelper org.kde.kup.runner root)

