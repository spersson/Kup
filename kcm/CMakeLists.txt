
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
${QT_QTCORE_LIBRARY}
${QT_QTDBUS_LIBRARY}
${QT_QTGUI_LIBRARY}
${KDE4_KDECORE_LIBRARY}
${KDE4_KDEUI_LIBRARY}
${KDE4_KIO_LIBRARY}
${KDE4_KFILE_LIBRARY}
${KDE4_SOLID_LIBRARY}
)

########### install files ###############

install(TARGETS kcm_kup DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kcm_kup.desktop DESTINATION ${SERVICES_INSTALL_DIR})

