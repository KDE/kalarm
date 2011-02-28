project(kalarm)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5950)
add_definitions(-DQT_STRICT_ITERATORS)

# Akonadi for KAlarm is still experimental.
option(KALARM_USE_AKONADI "Build to use Akonadi" FALSE)

# Kalarm from kdepim master needs kdepimlibs 4.7
if(NOT ${KDE_VERSION} VERSION_GREATER 4.6.40)
  set(KALARM_USE_AKONADI FALSE)
endif()

if(KALARM_USE_AKONADI)
    add_definitions(-DUSE_AKONADI)
else(KALARM_USE_AKONADI)
    add_definitions(-DWANT_DEPRECATED_KCAL_API)
    add_definitions(-DWANT_DEPRECATED_KABC_API)
    add_definitions(-DWANT_DEPRECATED_KRESOURCE_API)
endif(KALARM_USE_AKONADI)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")


if(KALARM_USE_AKONADI)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/cal
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_SOURCE_DIR}
    ${Boost_INCLUDE_DIR}
)
else(KALARM_USE_AKONADI)
include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/
    ${CMAKE_CURRENT_SOURCE_DIR}/cal
    ${CMAKE_CURRENT_SOURCE_DIR}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
    ${CMAKE_SOURCE_DIR}
    ${Boost_INCLUDE_DIR}
)
endif(KALARM_USE_AKONADI)

add_subdirectory(cal)
if(KALARM_USE_AKONADI)
    add_subdirectory(akonadi)
else(KALARM_USE_AKONADI)
    add_subdirectory(resources)
endif(KALARM_USE_AKONADI)
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
    calendarcompat.cpp
    commandoptions.cpp
    resourceselector.cpp
    templatepickdlg.cpp
    templatedlg.cpp
    templatemenuaction.cpp
)
if(KALARM_USE_AKONADI)
set(kalarm_bin_SRCS ${kalarm_bin_SRCS}
    akonadimodel.cpp
    collectionmodel.cpp
    itemlistmodel.cpp
   )
else(KALARM_USE_AKONADI)
set(kalarm_bin_SRCS ${kalarm_bin_SRCS}
    eventlistmodel.cpp
    alarmlistfiltermodel.cpp
    resourceconfigdialog.cpp
    resourcemodelview.cpp
    templatelistfiltermodel.cpp
   )
endif(KALARM_USE_AKONADI)

qt4_add_dbus_adaptor(kalarm_bin_SRCS org.kde.kalarm.kalarm.xml dbushandler.h DBusHandler)
if (KMAIL_SUPPORTED)
    qt4_add_dbus_interfaces(kalarm_bin_SRCS ${CMAKE_BINARY_DIR}/kmail/org.kde.kmail.kmail.xml)
endif (KMAIL_SUPPORTED)
qt4_add_dbus_interfaces(kalarm_bin_SRCS ${KDE4_DBUS_INTERFACES_DIR}/org.kde.KSpeech.xml)

if(KALARM_USE_AKONADI)
    qt4_add_dbus_interface(kalarm_bin_SRCS ${CMAKE_CURRENT_BINARY_DIR}/akonadi/kalarmdir/org.kde.Akonadi.KAlarmDir.Settings.xml kalarmdirsettings KAlarmDirSettings)
endif()

kde4_add_kcfg_files(kalarm_bin_SRCS GENERATE_MOC kalarmconfig.kcfgc)

#if (UNIX)
kde4_add_app_icon(kalarm_bin_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/appicons/ox*-app-kalarm.png")
kde4_add_executable(kalarm_bin ${kalarm_bin_SRCS})
if (KMAIL_SUPPORTED)
    add_dependencies(kalarm_bin kmail_xml)
endif (KMAIL_SUPPORTED)

set_target_properties(kalarm_bin PROPERTIES OUTPUT_NAME kalarm)

if(KALARM_USE_AKONADI)
    target_link_libraries(kalarm_bin kalarm_calendar ${KDEPIMLIBS_KCALCORE_LIBS} ${KDEPIMLIBS_KCALUTILS_LIBS} akonadi_next ${KDEPIMLIBS_KABC_LIBS} ${KDEPIMLIBS_KMIME_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_MAILTRANSPORT_LIBS} ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_AKONADI_KMIME_LIBS} ${KDEPIMLIBS_AKONADI_CONTACT_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS} ${KDEPIMLIBS_KPIMUTILS_LIBS} kdepim ${KDE4_KCMUTILS_LIBS} ${KDE4_PHONON_LIBS} ${X11_X11_LIB})
else(KALARM_USE_AKONADI)
    target_link_libraries(kalarm_bin kalarm_resources kalarm_calendar ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KABC_LIBS} ${KDEPIMLIBS_KMIME_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_MAILTRANSPORT_LIBS} ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_AKONADI_KMIME_LIBS} ${KDEPIMLIBS_AKONADI_CONTACT_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS} ${KDEPIMLIBS_KPIMUTILS_LIBS} kdepim ${KDE4_KCMUTILS_LIBS} ${KDE4_PHONON_LIBS} ${X11_X11_LIB})
endif(KALARM_USE_AKONADI)

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
