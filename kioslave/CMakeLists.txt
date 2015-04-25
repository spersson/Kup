
set(bupslave_SRCS
bupslave.cpp
bupvfs.cpp
vfshelpers.cpp
)

add_library(kio_bup MODULE ${bupslave_SRCS})
target_link_libraries(kio_bup
Qt5::Core
KF5::KIOCore
KF5::I18n
git24kup
)

install(TARGETS kio_bup DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES bup.protocol DESTINATION ${SERVICES_INSTALL_DIR})


add_definitions(-fexceptions)
