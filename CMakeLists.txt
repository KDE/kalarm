project(kalarm)

configure_file(config-kalarm.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kalarm.h )

include_directories( 
	${CMAKE_SOURCE_DIR}/kcal/
	${CMAKE_SOURCE_DIR}/kcal/libical/src/
	${CMAKE_BINARY_DIR}/kcal/libical/src/
	${CMAKE_SOURCE_DIR}/kalarm 
	${CMAKE_SOURCE_DIR}/kalarm/lib 
	${CMAKE_SOURCE_DIR}/kalarm/resources 
	${CMAKE_SOURCE_DIR}/libkleo 
	
) 

add_subdirectory( kalarmd ) 
add_subdirectory( lib ) 
add_subdirectory( resources ) 
add_subdirectory( pixmaps ) 


########### next target ###############
set(libkalarm_SRCS
	${CMAKE_SOURCE_DIR}/kalarm/lib/buttongroup.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/checkbox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/colourcombo.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/colourlist.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/combobox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/dateedit.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/datetime.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/label.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/messagebox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/pushbutton.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/radiobutton.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/timeedit.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/timespinbox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/timeperiod.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/timezonecombo.cpp
	${CMAKE_SOURCE_DIR}/kalarm/lib/shellprocess.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/slider.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/spinbox.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/spinbox2.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/lineedit.cpp 
	${CMAKE_SOURCE_DIR}/kalarm/lib/synchtimer.cpp
)

set(kalarm_bin_SRCS ${libkalarm_SRCS}
   birthdaydlg.cpp 
   main.cpp 
   alarmevent.cpp 
   editdlg.cpp 
   soundpicker.cpp 
   sounddlg.cpp 
   alarmcalendar.cpp 
   daemon.cpp 
   undo.cpp 
   kalarmapp.cpp 
   mainwindowbase.cpp 
   mainwindow.cpp 
   messagewin.cpp 
   preferences.cpp 
   prefdlg.cpp 
   traywindow.cpp 
   dbushandler.cpp 
   recurrenceedit.cpp 
   karecurrence.cpp 
   deferdlg.cpp 
   functions.cpp 
   fontcolour.cpp 
   fontcolourbutton.cpp 
   alarmtimewidget.cpp 
   specialactions.cpp 
   reminder.cpp 
   startdaytimer.cpp 
   eventlistviewbase.cpp 
   alarmlistview.cpp 
   kamail.cpp 
   timeselector.cpp 
   templatelistview.cpp 
   templatepickdlg.cpp 
   templatedlg.cpp 
   templatemenuaction.cpp 
   latecancel.cpp 
   repetition.cpp 
   alarmtext.cpp 
   emailidcombo.cpp 
   find.cpp 
   pickfileradio.cpp 
   calendarcompat.cpp
   resourceconfigdialog.cpp
   resourceselector.cpp
)
qt4_add_dbus_adaptor( kalarm_bin_SRCS org.kde.kalarm.kalarm.xml dbushandler.h DBusHandler )
qt4_add_dbus_adaptor( kalarm_bin_SRCS org.kde.kalarm.notify.xml daemon_p.h NotificationHandler )
qt4_add_dbus_interfaces(kalarm_bin_SRCS kalarmd/org.kde.kalarm.daemon.Daemon.xml )
qt4_add_dbus_interfaces(kalarm_bin_SRCS ${CMAKE_SOURCE_DIR}/kmail/org.kde.kmail.kmail.xml )

kde4_automoc(${kalarm_bin_SRCS})

kde4_add_executable(kalarm_bin ${kalarm_bin_SRCS})
set_target_properties(kalarm_bin PROPERTIES OUTPUT_NAME kalarm)

target_link_libraries(kalarm_bin ${KDE4_KUTILS_LIBS} kalarm_resources ${KDE4_KCAL_LIBS} ${KDE4_KABC_LIBS} ${KDE4_KMIME_LIBS} kdepim kpimidentities phononcore )

install(TARGETS kalarm_bin  DESTINATION ${BIN_INSTALL_DIR} )


########### install files ###############

install( FILES kalarm.desktop  DESTINATION ${XDG_APPS_DIR} )
install( FILES kalarm.tray.desktop  DESTINATION ${AUTOSTART_INSTALL_DIR} )
install( FILES kalarmui.rc  DESTINATION ${DATA_INSTALL_DIR}/kalarm )
install( FILES org.kde.kalarm.notify.xml org.kde.kalarm.kalarm.xml DESTINATION ${DBUS_INTERFACES_DIR} )


kde4_install_icons( ${ICON_INSTALL_DIR}   )
