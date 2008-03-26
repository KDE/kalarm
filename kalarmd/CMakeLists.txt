
include_directories( ../  )


########### next target ###############

set(kalarmd_SRCS 
	alarmdaemon.cpp 
	admain.cpp 
	adapp.cpp 
)

qt4_add_dbus_adaptor( kalarmd_SRCS org.kde.kalarm.kalarmd.Daemon.xml alarmdaemon.h AlarmDaemon )

kde4_add_app_icon(kalarmd_SRCS "../ox*-app-kalarm.png")

kde4_add_executable(kalarmd ${kalarmd_SRCS})

target_link_libraries(kalarmd ${KDE4_KDEUI_LIBS} kalarm_resources ${KDE4_KCAL_LIBS} )

install(TARGETS kalarmd  DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES kalarmd.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR})
install( FILES kalarmd.autostart.desktop  DESTINATION ${AUTOSTART_INSTALL_DIR})
install( FILES org.kde.kalarm.kalarmd.Daemon.xml DESTINATION ${KDE4_DBUS_INTERFACES_DIR})
