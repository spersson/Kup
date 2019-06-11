set(resticcore_SRCS
restichelper.cpp
resticforgetswitch.cpp
)

ecm_qt_declare_logging_category(resticcore_SRCS
    HEADER kupresticcore_debug.h
    IDENTIFIER KUPRESTICCORE
    CATEGORY_NAME kup.resticcore
    DEFAULT_SEVERITY Debug
)

add_definitions(-fexceptions)

add_library(resticcore STATIC ${resticcore_SRCS})
target_link_libraries(resticcore
Qt5::Core
KF5::CoreAddons
KF5::Pty
KF5::I18n
)
