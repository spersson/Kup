
include_directories(${KDE4_INCLUDES})
include_directories("../settings")
include_directories("../daemon")

set(kcm_kup_SRCS
backupplanwidget.cpp
driveselection.cpp
driveselectiondelegate.cpp
folderselectionmodel.cpp
kupkcm.cpp
planstatuswidget.cpp
../settings/backupplan.cpp
../settings/kupsettings.cpp
)

kde4_add_plugin(kcm_kup ${kcm_kup_SRCS})

target_link_libraries(kcm_kup
${KDE4_KDEUI_LIBS}
${KDE4_KFILE_LIBS}
${KDE4_SOLID_LIBS}
)

########### install files ###############

install(TARGETS kcm_kup DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kcm_kup.desktop DESTINATION ${SERVICES_INSTALL_DIR})

