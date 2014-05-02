project(kalarm)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5950)
add_definitions(-DQT_STRICT_ITERATORS)

add_definitions( -DQT_NO_CAST_FROM_ASCII )
add_definitions( -DQT_NO_CAST_TO_ASCII )

set(CMAKE_AUTOMOC_RELAXED_MODE ON)

    add_definitions(-DUSE_AKONADI)
    if (NOT CMAKE_VERSION VERSION_LESS 2.8.12 AND (CMAKE_VERSION VERSION_LESS 2.8.12.1
            OR CMAKE_VERSION VERSION_LESS 2.8.13 # Temporary measure because build.kde.org uses a datestamped CMake version
            ))
        # http://public.kitware.com/Bug/view.php?id=14535
        set(CMAKE_AUTOMOC_MOC_OPTIONS -DUSE_AKONADI)
    endif()
kde_enable_exceptions()


include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libkdepim/
    ${Boost_INCLUDE_DIR}
)

add_subdirectory(appicons)
add_subdirectory(pixmaps)
add_subdirectory(autostart)


########### next target ###############
set(libkalarm_SRCS
    lib/buttongroup.cpp
    lib/checkbox.cpp
    lib/colourbutton.cpp
    lib/combobox.cpp
    lib/desktop.cpp
    lib/filedialog.cpp
    lib/groupbox.cpp
    lib/itembox.cpp
    lib/kalocale.cpp
    lib/label.cpp
    #REACTIVATE lib/messagebox.cpp
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
    alarmtime.cpp
    alarmtimewidget.cpp
    specialactions.cpp
    reminder.cpp
    startdaytimer.cpp
    eventlistview.cpp
    alarmlistdelegate.cpp
    alarmlistview.cpp
    templatelistview.cpp
    kamail.cpp
    timeselector.cpp
    latecancel.cpp
    repetitionbutton.cpp
    emailidcombo.cpp
    find.cpp
    pickfileradio.cpp
    newalarmaction.cpp
    commandoptions.cpp
    resourceselector.cpp
    templatepickdlg.cpp
    templatedlg.cpp
    templatemenuaction.cpp
    wakedlg.cpp
)
set(kalarm_bin_SRCS ${kalarm_bin_SRCS}
    akonadimodel.cpp
    akonadiresourcecreator.cpp
    collectionmodel.cpp
    collectionsearch.cpp
    itemlistmodel.cpp
    calendarmigrator.cpp
    eventid.cpp
   )

qt5_wrap_ui(kalarm_bin_SRCS
    wakedlg.ui
)

qt5_add_dbus_adaptor(kalarm_bin_SRCS org.kde.kalarm.kalarm.xml dbushandler.h DBusHandler)
if (KMAIL_SUPPORTED)
    qt5_add_dbus_interfaces(kalarm_bin_SRCS ${CMAKE_BINARY_DIR}/kmail/org.kde.kmail.kmail.xml)
endif ()
#REACTIVATE qt5_add_dbus_interfaces(kalarm_bin_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.kde.KSpeech.xml)

kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kalarmresource.kcfg org.kde.Akonadi.KAlarm.Settings)
qt5_add_dbus_interface(kalarm_bin_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarm.Settings.xml kalarmsettings KAlarmSettings)

kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kalarmdirresource.kcfg org.kde.Akonadi.KAlarmDir.Settings)
qt5_add_dbus_interface(kalarm_bin_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarmDir.Settings.xml kalarmdirsettings KAlarmDirSettings)

qt5_add_dbus_interfaces(kalarm_bin_SRCS ${AKONADI_DBUS_INTERFACES_DIR}/org.freedesktop.Akonadi.Agent.Control.xml)
#qt5_add_dbus_adaptor(kalarm_bin_SRCS ${AKONADI_DBUS_INTERFACES_DIR}/org.freedesktop.Akonadi.Agent.Control.xml agentbase.h Akonadi::AgentBase controladaptor Akonadi__ControlAdaptor)

kconfig_add_kcfg_files(kalarm_bin_SRCS GENERATE_MOC kalarmconfig.kcfgc)

#if (UNIX)
kde4_add_app_icon(kalarm_bin_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/appicons/ox*-app-kalarm.png")
add_executable(kalarm_bin ${kalarm_bin_SRCS})
add_dependencies(kalarm_bin kalarm_resource_xml kalarmdir_resource_xml)
if (KMAIL_SUPPORTED)
    add_dependencies(kalarm_bin kmail_xml)
endif ()

set_target_properties(kalarm_bin PROPERTIES OUTPUT_NAME kalarm)

target_link_libraries(kalarm_bin
KF5::AlarmCalendar
KF5::CalendarCore
KF5::CalendarUtils
KF5::Abc
KF5::Mime
KF5::PimIdentities
KF5::MailTransport
KF5::AkonadiCore
KF5::AkonadiMime
KF5::AkonadiContact
KF5::Holidays
KF5::PimUtils
kdepim
KF5::KCMUtils
${KDE4_PHONON_LIBS}
${X11_X11_LIB}
)


install(TARGETS kalarm_bin ${INSTALL_TARGETS_DEFAULT_ARGS})
#endif (UNIX)

########### install files ###############

install(PROGRAMS kalarm.desktop  DESTINATION ${XDG_APPS_INSTALL_DIR})
install(PROGRAMS kalarm.autostart.desktop DESTINATION ${AUTOSTART_INSTALL_DIR})
install(FILES kalarmconfig.kcfg DESTINATION ${KCFG_INSTALL_DIR})
install(FILES kalarmui.rc DESTINATION ${DATA_INSTALL_DIR}/kalarm)
install(FILES org.kde.kalarm.kalarm.xml DESTINATION ${DBUS_INTERFACES_INSTALL_DIR})
install(FILES kalarm.upd DESTINATION ${KCONF_UPDATE_INSTALL_DIR})
install(PROGRAMS kalarm-version.pl kalarm-1.2.1-general.pl kalarm-1.9.5-defaults.pl kalarm-2.0.2-general.pl kalarm-2.1.5-general.pl DESTINATION ${KCONF_UPDATE_INSTALL_DIR})

########### KAuth helper ###############

add_executable(kalarm_helper rtcwakeaction.cpp)
target_link_libraries(kalarm_helper )
install(TARGETS kalarm_helper DESTINATION ${LIBEXEC_INSTALL_DIR})
kde4_install_auth_helper_files(kalarm_helper org.kde.kalarmrtcwake root)
#PORT TO QT5
#kde4_install_auth_actions(org.kde.kalarmrtcwake rtcwakeaction.actions)
