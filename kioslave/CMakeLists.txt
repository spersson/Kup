
set(bupslave_SRCS
bupslave.cpp
bupvfs.cpp
vfshelpers.cpp
)

ecm_qt_declare_logging_category(bupslave_SRCS
    HEADER kupkio_debug.h
    IDENTIFIER KUPKIO
    CATEGORY_NAME kup.kio
    DEFAULT_SEVERITY Debug
)

add_library(kio_bup MODULE ${bupslave_SRCS})
target_link_libraries(kio_bup
Qt5::Core
KF5::KIOCore
KF5::I18n
${libgit_link_name}
)

install(TARGETS kio_bup DESTINATION ${PLUGIN_INSTALL_DIR})
install(FILES bup.protocol DESTINATION ${SERVICES_INSTALL_DIR})

add_definitions(-fexceptions)
