include_directories(${KDE4_INCLUDES})

set(bupslave_SRCS
bupslave.cpp
bupvfs.cpp
vfshelpers.cpp
)

kde4_add_plugin(kio_bup ${bupslave_SRCS})
target_link_libraries(kio_bup
${QT_QTCORE_LIBRARY}
${KDE4_KDECORE_LIBRARY}
${KDE4_KIO_LIBRARY}
git24kup
)

install(TARGETS kio_bup DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES bup.protocol DESTINATION ${SERVICES_INSTALL_DIR})


add_definitions(-fexceptions)
