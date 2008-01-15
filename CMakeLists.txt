project(kalarm)

include_directories(
        ./ 
	./lib 
	./resources 
) 

add_subdirectory( kalarmd ) 
add_subdirectory( resources ) 
add_subdirectory( pixmaps ) 


########### next target ###############
set(libkalarm_SRCS
	lib/buttongroup.cpp 
	lib/checkbox.cpp 
	lib/colourcombo.cpp 
	lib/colourlist.cpp 
	lib/combobox.cpp 
	lib/dateedit.cpp 
	lib/datetime.cpp 
	lib/label.cpp 
	lib/messagebox.cpp 
	lib/packedlayout.cpp 
	lib/pushbutton.cpp 
	lib/radiobutton.cpp 
	lib/timeedit.cpp 
	lib/timespinbox.cpp 
	lib/timeperiod.cpp 
	lib/timezonecombo.cpp
	lib/shellprocess.cpp 
	lib/slider.cpp 
	lib/spinbox.cpp 
	lib/spinbox2.cpp 
	lib/lineedit.cpp 
	lib/synchtimer.cpp
)

set(kalarm_bin_SRCS ${libkalarm_SRCS}
   birthdaydlg.cpp 
   birthdaymodel.cpp
   main.cpp 
   alarmevent.cpp 
   editdlg.cpp 
   editdlgtypes.cpp
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
   eventlistview.cpp 
   eventlistmodel.cpp
   alarmlistfiltermodel.cpp
   alarmlistdelegate.cpp
   alarmlistview.cpp 
   templatelistfiltermodel.cpp
   templatelistview.cpp 
   templatepickdlg.cpp 
   templatedlg.cpp 
   templatemenuaction.cpp 
   kamail.cpp 
   timeselector.cpp 
   latecancel.cpp 
   repetition.cpp 
   alarmtext.cpp 
   emailidcombo.cpp 
   find.cpp 
   pickfileradio.cpp 
   newalarmaction.cpp
   calendarcompat.cpp
   resourceconfigdialog.cpp
   resourcemodelview.cpp
   resourceselector.cpp
)
qt4_add_dbus_adaptor( kalarm_bin_SRCS org.kde.kalarm.kalarm.xml dbushandler.h DBusHandler )
qt4_add_dbus_adaptor( kalarm_bin_SRCS org.kde.kalarm.notify.xml daemon_p.h NotificationHandler )
qt4_add_dbus_interfaces(kalarm_bin_SRCS kalarmd/org.kde.kalarm.kalarmd.Daemon.xml )
qt4_add_dbus_interfaces(kalarm_bin_SRCS ${CMAKE_BINARY_DIR}/kmail/org.kde.kmail.kmail.xml )

kde4_add_kcfg_files(kalarm_bin_SRCS GENERATE_MOC kalarmconfig.kcfgc)

if (UNIX)
kde4_add_executable(kalarm_bin ${kalarm_bin_SRCS})
add_dependencies(kalarm_bin kmail_xml)

set_target_properties(kalarm_bin PROPERTIES OUTPUT_NAME kalarm)

kdepim4_link_unique_libraries(kalarm_bin ${KDE4_KUTILS_LIBS} kalarm_resources ${KDE4_KCAL_LIBS} ${KDE4_KABC_LIBRARY} ${KDE4_KMIME_LIBRARY} ${KDE4_KPIMIDENTITIES_LIBS} ${KDE4_MAILTRANSPORT_LIBS} ${KDE4_PHONON_LIBS} kdepim)


install(TARGETS kalarm_bin DESTINATION ${BIN_INSTALL_DIR} )
endif (UNIX)

########### install files ###############

install( FILES kalarm.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES kalarm.tray.desktop  DESTINATION ${AUTOSTART_INSTALL_DIR} )
install( FILES kalarmconfig.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install( FILES kalarmui.rc DESTINATION ${DATA_INSTALL_DIR}/kalarm )
install( FILES org.kde.kalarm.notify.xml org.kde.kalarm.kalarm.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR} )
install( FILES kalarm.upd DESTINATION ${KCONF_UPDATE_INSTALL_DIR})
install( PROGRAMS kalarm-version.pl kalarm-1.2.1-general.pl kalarm-1.9.5-defaults.pl DESTINATION ${KCONF_UPDATE_INSTALL_DIR})

kde4_install_icons( ${ICON_INSTALL_DIR} )
