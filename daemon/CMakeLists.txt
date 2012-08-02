
include_directories(${KDE4_INCLUDES})
include_directories("../settings")

set(kupdaemon_SRCS
main.cpp
kupdaemon.cpp
planexecutor.cpp
edexecutor.cpp
fsexecutor.cpp
bupjob.cpp
)

kde4_add_executable(kupdaemon ${kupdaemon_SRCS})

target_link_libraries(kupdaemon
kupsettings
${KDE4_KDEUI_LIBS}
${KDE4_KFILE_LIBS}
${KDE4_SOLID_LIBS}
${KDE4_KIDLETIME_LIBS}
)

########### install files ###############
install(TARGETS kupdaemon ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES kupdaemon.desktop DESTINATION ${AUTOSTART_INSTALL_DIR})
install(FILES kupdaemon.notifyrc DESTINATION ${DATA_INSTALL_DIR}/kupdaemon)

