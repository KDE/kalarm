include_directories( ../  )

########### next target ###############

set(kalarmautostart_SRCS 
	autostart.cpp
        kalarm_autostart_debug.cpp 
)

file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../appicons/*-apps-kalarm.png")
ecm_add_app_icon(kalarmautostart_SRCS ICONS ${ICONS_SRCS})

add_executable(kalarmautostart ${kalarmautostart_SRCS})

target_link_libraries(kalarmautostart  KF5::I18n KF5::CoreAddons Qt5::Widgets KF5::KDELibs4Support)

install(TARGETS kalarmautostart  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
