include_directories(${KDE4_INCLUDES})

set(bupslave_SRCS
    bupslave.cpp
)

kde4_add_plugin(kio_bup ${bupslave_SRCS})
target_link_libraries(kio_bup ${KDE4_KIO_LIBS})

install(TARGETS kio_bup DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES bup.protocol DESTINATION ${SERVICES_INSTALL_DIR})

