include_directories(${KDE4_INCLUDES})

set(bupslave_SRCS
bupslave.cpp
bupvfs.cpp
vfshelpers.cpp
)

kde4_add_plugin(kio_bup ${bupslave_SRCS})
target_link_libraries(kio_bup
#${KDE4_KIO_LIBS}
# save some unneeded linking by naming the needed libraries directly
QtCore
kdecore
kio
git2
)

install(TARGETS kio_bup DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES bup.protocol DESTINATION ${SERVICES_INSTALL_DIR})


add_definitions(-fexceptions)

#set(bupkio_SRCS
#bupkio.cpp
#bupvfs.cpp
#vfshelpers.cpp
#)

#kde4_add_executable(bupkio ${bupkio_SRCS})
#target_link_libraries(bupkio
#${KDE4_KDECORE_LIBRARY}
#git2
#)
