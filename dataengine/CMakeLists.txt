#this is a library so it needs to enforce it's translation domain, not use the application's domain.
add_definitions(-DTRANSLATION_DOMAIN="kup")
include_directories("../daemon")

# Plasma Data Engine
set(plasma_engine_kup_SRCS
	kupengine.cpp
	kupservice.cpp
	kupjob.cpp
	)
add_library(plasma_engine_kup MODULE ${plasma_engine_kup_SRCS})
target_link_libraries(plasma_engine_kup
	Qt5::Network
	KF5::Plasma
)

kcoreaddons_desktop_to_json(plasma_engine_kup plasma-dataengine-kup.desktop)

install(TARGETS plasma_engine_kup DESTINATION ${KDE_INSTALL_PLUGINDIR}/plasma/dataengine)
install(FILES plasma-dataengine-kup.desktop DESTINATION ${KDE_INSTALL_KSERVICES5DIR})
install(FILES kupservice.operations DESTINATION ${PLASMA_DATA_INSTALL_DIR}/services)

