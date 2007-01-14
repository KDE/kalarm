
include_directories( ${CMAKE_SOURCE_DIR}/kalarm  )


########### next target ###############

set(kalarmd_SRCS 
	alarmdaemon.cpp 
	admain.cpp 
	adapp.cpp 
)

qt4_add_dbus_adaptor( kalarmd_SRCS org.kde.kalarm.daemon.Daemon.xml alarmdaemon.h AlarmDaemon )
qt4_add_dbus_interfaces( kalarmd_SRCS ../org.kde.kalarm.notify.xml )


kde4_automoc(${kalarmd_SRCS})

kde4_add_executable(kalarmd ${kalarmd_SRCS})

target_link_libraries(kalarmd ${KDE4_KDEUI_LIBS} kalarm_resources ${KDE4_KCAL_LIBS} )

install(TARGETS kalarmd  DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES kalarmd.desktop  DESTINATION ${XDG_APPS_DIR})
install( FILES kalarmd.autostart.desktop  DESTINATION ${AUTOSTART_INSTALL_DIR})
install( FILES org.kde.kalarm.daemon.Daemon.xml DESTINATION ${DBUS_INTERFACES_DIR})
