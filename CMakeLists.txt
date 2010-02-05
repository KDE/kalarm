project(kalarm)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5950)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")

include_directories(
${CMAKE_CURRENT_BINARY_DIR}
        ./ 
	./cal
	./lib 
	./resources 
        ${Boost_INCLUDE_DIR}
) 

add_subdirectory( cal ) 

# KAlarm depends on kdepim/akonadi/ which is impossible for packagers.
# The Akonadi resource for KAlarm won't be available in KDE 4.4
if( BUILD_akonadi )
# Disable for KDE 4.4 since it doesn't work yet
#  add_subdirectory( akonadi )
endif( BUILD_akonadi )

add_subdirectory( resources ) 
add_subdirectory( appicons ) 
add_subdirectory( pixmaps ) 
add_subdirectory( autostart ) 


########### next target ###############
set(libkalarm_SRCS
	lib/buttongroup.cpp 
	lib/checkbox.cpp 
	lib/colourbutton.cpp 
	lib/combobox.cpp 
	lib/dateedit.cpp 
	lib/datetime.cpp 
	lib/desktop.cpp
	lib/filedialog.cpp
	lib/itembox.cpp
	lib/kalocale.cpp
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
	lib/stackedwidgets.cpp
	lib/lineedit.cpp 
	lib/synchtimer.cpp
)

set(kalarm_bin_SRCS ${libkalarm_SRCS}
   birthdaydlg.cpp 
   birthdaymodel.cpp
   main.cpp 
   kaevent.cpp 
   editdlg.cpp 
   editdlgtypes.cpp
   soundpicker.cpp 
   sounddlg.cpp 
   alarmcalendar.cpp 
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
   repetitionbutton.cpp 
   emailidcombo.cpp 
   find.cpp 
   pickfileradio.cpp 
   newalarmaction.cpp
   calendarcompat.cpp
   resourceconfigdialog.cpp
   resourcemodelview.cpp
   resourceselector.cpp
   commandoptions.cpp
)
qt4_add_dbus_adaptor(kalarm_bin_SRCS org.kde.kalarm.kalarm.xml dbushandler.h DBusHandler)
if (KMAIL_SUPPORTED)
   qt4_add_dbus_interfaces(kalarm_bin_SRCS ${CMAKE_BINARY_DIR}/kmail/org.kde.kmail.kmail.xml)
endif (KMAIL_SUPPORTED)
qt4_add_dbus_interfaces(kalarm_bin_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.kde.KSpeech.xml)

if( BUILD_akonadi )
  kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/akonadi/kalarmresource.kcfg org.kde.Akonadi.KAlarm.Settings)
  qt4_add_dbus_interface(kalarm_bin_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarm.Settings.xml kalarmresourcesettings KAlarmResourceSettings)
endif( BUILD_akonadi )

kde4_add_kcfg_files(kalarm_bin_SRCS GENERATE_MOC kalarmconfig.kcfgc)

#if (UNIX)
kde4_add_app_icon(kalarm_bin_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/appicons/ox*-app-kalarm.png")
kde4_add_executable(kalarm_bin ${kalarm_bin_SRCS})
if (KMAIL_SUPPORTED)
   add_dependencies(kalarm_bin kmail_xml)
endif (KMAIL_SUPPORTED)

set_target_properties(kalarm_bin PROPERTIES OUTPUT_NAME kalarm)

target_link_libraries(kalarm_bin ${KDE4_KUTILS_LIBS} kalarm_resources kalarm_calendar ${KDEPIMLIBS_KCAL_LIBS} ${KDE4_KABC_LIBRARY} ${KDE4_KMIME_LIBRARY} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_MAILTRANSPORT_LIBS} ${KDEPIMLIBS_AKONADI_KMIME_LIBRARY} ${KDEPIMLIBS_KHOLIDAYS_LIBS} ${KDE4_PHONON_LIBS} ${X11_X11_LIB} kdepim)

install(TARGETS kalarm_bin ${INSTALL_TARGETS_DEFAULT_ARGS} )
#endif (UNIX)

########### install files ###############

install( FILES kalarm.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR} )
install( FILES kalarm.autostart.desktop DESTINATION ${AUTOSTART_INSTALL_DIR} )
install( FILES kalarmconfig.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install( FILES kalarmui.rc DESTINATION ${DATA_INSTALL_DIR}/kalarm )
install( FILES org.kde.kalarm.kalarm.xml DESTINATION ${KDE4_DBUS_INTERFACES_DIR} )
install( FILES kalarm.upd DESTINATION ${KCONF_UPDATE_INSTALL_DIR})
install( PROGRAMS kalarm-version.pl kalarm-1.2.1-general.pl kalarm-1.9.5-defaults.pl kalarm-2.0.2-general.pl kalarm-2.1.5-general.pl DESTINATION ${KCONF_UPDATE_INSTALL_DIR})
