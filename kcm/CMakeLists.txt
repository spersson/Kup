
include_directories("../settings")
include_directories("../daemon")

set(kcm_kup_SRCS
backupplanwidget.cpp
dirselector.cpp
driveselection.cpp
driveselectiondelegate.cpp
folderselectionmodel.cpp
kupkcm.cpp
planstatuswidget.cpp
kbuttongroup.cpp
../settings/backupplan.cpp
../settings/kupsettings.cpp
)

add_library(kcm_kup MODULE ${kcm_kup_SRCS})

target_link_libraries(kcm_kup
Qt5::Core
Qt5::DBus
Qt5::Gui
KF5::KIOCore
KF5::KIOFileWidgets
KF5::Solid
KF5::I18n
)

########### install files ###############

install(TARGETS kcm_kup DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES kcm_kup.desktop DESTINATION ${SERVICES_INSTALL_DIR})

