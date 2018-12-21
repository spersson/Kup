include_directories("../daemon")
include_directories("../kcm")
include_directories("../kioslave")
include_directories("../settings")

set(filedigger_SRCS
filedigger.cpp
main.cpp
mergedvfs.cpp
mergedvfsmodel.cpp
restoredialog.cpp
restorejob.cpp
versionlistdelegate.cpp
versionlistmodel.cpp
../kioslave/vfshelpers.cpp
../kcm/dirselector.cpp
../settings/kuputils.cpp
)

ecm_qt_declare_logging_category(filedigger_SRCS
    HEADER kupfiledigger_debug.h
    IDENTIFIER KUPFILEDIGGER
    CATEGORY_NAME kup.filedigger
    DEFAULT_SEVERITY Warning
)

add_definitions(-fexceptions)

ki18n_wrap_ui(filedigger_SRCS restoredialog.ui)
add_executable(kup-filedigger ${filedigger_SRCS})
target_link_libraries(kup-filedigger
Qt5::Core
Qt5::Gui
KF5::KIOCore
KF5::KIOFileWidgets
KF5::I18n
KF5::IconThemes
KF5::JobWidgets
${libgit_link_name}
)

########### install files ###############
install(TARGETS kup-filedigger ${INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES filedigger.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR})
