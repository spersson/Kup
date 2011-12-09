
include_directories(${KDE4_INCLUDES})
include_directories("../settings")
include_directories("../bup")

set(kuprunner_SRCS
main.cpp
kuprunner.cpp
)

kde4_add_executable(kuprunner ${kuprunner_SRCS})

target_link_libraries(kuprunner
kupsettings
bupinterface
${KDE4_KDEUI_LIBS}
# ${KDE4_KFILE_LIBS}
# ${KDE4_SOLID_LIBS}
#${KDE4_KIO_LIBS}
)

#qt4_add_dbus_adaptor(boombox_SRCS dbus/org.mpris.MediaPlayer2.Player.xml
#                     mpris2player.h Mpris2Player)
#qt4_add_dbus_interface(boombox_SRCS dbus/org.mpris.MediaPlayer2.Player.xml
#                     mpris2playerclient)

# add_definitions(${TAGLIB_CFLAGS} ${SQLITE_DEFINITIONS})


########### install files ###############
install(TARGETS kuprunner ${INSTALL_TARGETS_DEFAULT_ARGS})
# install(FILES kupdaemon.desktop DESTINATION ${AUTOSTART_INSTALL_DIR})
# install(FILES kupdaemon.notifyrc DESTINATION ${DATA_INSTALL_DIR}/kupdaemon)

#install(FILES org.kde.kup.xml DESTINATION ${CMAKE_INSTALL_PREFIX}/share/dbus-1/interfaces)
