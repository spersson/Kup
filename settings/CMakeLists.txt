
include_directories(${KDE4_INCLUDES})

set(kupsettings_SRCS
kupsettings.cpp
backupplan.cpp
)

kde4_add_library(kupsettings STATIC ${kupsettings_SRCS})
target_link_libraries(kupsettings ${KDE4_KDECORE_LIBS})
