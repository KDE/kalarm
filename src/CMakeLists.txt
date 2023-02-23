include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/
)

add_subdirectory(kalarmcalendar)
if (ENABLE_AKONADI_PLUGIN)
    add_subdirectory(akonadiplugin)
endif()
add_subdirectory(appicons)
add_subdirectory(pixmaps)
add_subdirectory(autostart)

########### next target ###############


add_library(kalarmplugin)

ecm_qt_declare_logging_category(kalarmplugin
                                HEADER kalarmpluginlib_debug.h
                                IDENTIFIER KALARMPLUGINLIB_LOG
                                CATEGORY_NAME org.kde.pim.kalarm.pluginlib
                                DESCRIPTION "kalarm (pluginlib)"
                                EXPORT KALARMPLUGINLIB
                                )
target_sources(kalarmplugin PRIVATE
    pluginbase.cpp
    pluginmanager.cpp
    pluginbase.h
    pluginmanager.h
)
generate_export_header(kalarmplugin BASE_NAME kalarmpluginlib)

target_link_libraries(kalarmplugin PRIVATE
    kalarmcalendar
    KF${KF_MAJOR_VERSION}::CoreAddons
    KF${KF_MAJOR_VERSION}::CalendarCore
    KPim${KF_MAJOR_VERSION}::Mime
)

target_include_directories(kalarmplugin PUBLIC "$<BUILD_INTERFACE:${kalarm_SOURCE_DIR}/src/kalarmcalendar;${kalarm_BINARY_DIR}/src/kalarmcalendar>")

set_target_properties(kalarmplugin
    PROPERTIES VERSION ${KDEPIM_LIB_VERSION}
    SOVERSION ${KDEPIM_LIB_SOVERSION}
    )

install(TARGETS kalarmplugin ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)


########### next target ###############

set(libkalarm_common_SRCS)

ecm_qt_declare_logging_category(libkalarm_common_SRCS
                                HEADER kalarm_debug.h
                                IDENTIFIER KALARM_LOG
                                CATEGORY_NAME org.kde.pim.kalarm
                                DEFAULT_SEVERITY Warning
                                DESCRIPTION "kalarm (kalarm)"
                                OLD_CATEGORY_NAMES log_kalarm
                                EXPORT KALARM
                                )

set(libkalarm_SRCS
    lib/buttongroup.cpp
    lib/checkbox.cpp
    lib/colourbutton.cpp
    lib/combobox.cpp
    lib/config.cpp
    lib/desktop.cpp
    lib/dragdrop.cpp
    lib/file.cpp
    lib/filedialog.cpp
    lib/groupbox.cpp
    lib/label.cpp
    lib/locale.cpp
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
    lib/buttongroup.h
    lib/checkbox.h
    lib/colourbutton.h
    lib/combobox.h
    lib/config.h
    lib/desktop.h
    lib/dragdrop.h
    lib/file.h
    lib/filedialog.h
    lib/groupbox.h
    lib/label.h
    lib/locale.h
    lib/messagebox.h
    lib/packedlayout.h
    lib/pushbutton.h
    lib/radiobutton.h
    lib/timeedit.h
    lib/timespinbox.h
    lib/timeperiod.h
    lib/timezonecombo.h
    lib/shellprocess.h
    lib/slider.h
    lib/spinbox.h
    lib/spinbox2.h
    lib/stackedwidgets.h
    lib/lineedit.h
    lib/synchtimer.h
   )
set(resources_SRCS
    resources/calendarfunctions.cpp
    resources/resourcetype.cpp
    resources/resource.cpp
    resources/resources.cpp
    resources/resourcedatamodelbase.cpp
    resources/resourcemodel.cpp
    resources/resourceselectdialog.cpp
    resources/resourcecreator.cpp
    resources/eventmodel.cpp
    resources/datamodel.cpp
    resources/calendarupdater.cpp
    resources/fileresource.cpp
    resources/fileresourceconfigmanager.cpp
    resources/fileresourcecreator.cpp
    resources/fileresourcedatamodel.cpp
    resources/fileresourcesettings.cpp
    resources/fileresourcecalendarupdater.cpp
    resources/singlefileresource.cpp
    resources/singlefileresourceconfigdialog.cpp
    resources/migration/dirresourceimportdialog.cpp
    resources/migration/fileresourcemigrator.cpp
    resources/calendarfunctions.h
    resources/resourcetype.h
    resources/resource.h
    resources/resources.h
    resources/resourcedatamodelbase.h
    resources/resourcemodel.h
    resources/resourceselectdialog.h
    resources/resourcecreator.h
    resources/eventmodel.h
    resources/datamodel.h
    resources/calendarupdater.h
    resources/fileresource.h
    resources/fileresourceconfigmanager.h
    resources/fileresourcecreator.h
    resources/fileresourcedatamodel.h
    resources/fileresourcesettings.h
    resources/fileresourcecalendarupdater.h
    resources/singlefileresource.h
    resources/singlefileresourceconfigdialog.h
    resources/migration/dirresourceimportdialog.h
    resources/migration/fileresourcemigrator.h
   )

set(kalarm_bin_SRCS ${libkalarm_SRCS} ${resources_SRCS}
    ${libkalarm_common_SRCS}
    main.cpp
    birthdaydlg.cpp
    editdlg.cpp
    editdlgtypes.cpp
    soundpicker.cpp
    sounddlg.cpp
    displaycalendar.cpp
    resourcescalendar.cpp
    undo.cpp
    kalarmapp.cpp
    mainwindowbase.cpp
    mainwindow.cpp
    messagedisplay.cpp
    messagedisplayhelper.cpp
    messagenotification.cpp
    messagewindow.cpp
    preferences.cpp
    prefdlg.cpp
    traywindow.cpp
    dbushandler.cpp
    recurrenceedit.cpp
    deferdlg.cpp
    eventid.cpp
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
    commandoptions.cpp
    resourceselector.cpp
    datepicker.cpp
    daymatrix.cpp
    templatepickdlg.cpp
    templatedlg.cpp
    templatemenuaction.cpp
    migratekde4files.cpp
    birthdaydlg.h
    editdlg.h
    editdlgtypes.h
    soundpicker.h
    sounddlg.h
    displaycalendar.h
    resourcescalendar.h
    undo.h
    kalarmapp.h
    mainwindowbase.h
    mainwindow.h
    messagedisplay.h
    messagedisplayhelper.h
    messagenotification.h
    messagewindow.h
    preferences.h
    prefdlg.h
    traywindow.h
    dbushandler.h
    recurrenceedit.h
    deferdlg.h
    eventid.h
    functions.h
    fontcolour.h
    fontcolourbutton.h
    alarmtimewidget.h
    specialactions.h
    reminder.h
    startdaytimer.h
    eventlistview.h
    alarmlistdelegate.h
    alarmlistview.h
    templatelistview.h
    kamail.h
    timeselector.h
    latecancel.h
    repetitionbutton.h
    emailidcombo.h
    find.h
    pickfileradio.h
    newalarmaction.h
    commandoptions.h
    resourceselector.h
    datepicker.h
    daymatrix.h
    templatepickdlg.h
    templatedlg.h
    templatemenuaction.h
    migratekde4files.h
)
if (ENABLE_WAKE_FROM_SUSPEND)
    set(kalarm_bin_SRCS ${kalarm_bin_SRCS}
        wakedlg.cpp
        wakedlg.h
    )
endif()

ki18n_wrap_ui(kalarm_bin_SRCS
    wakedlg.ui
    resources/singlefileresourceconfigdialog.ui
    resources/migration/dirresourceimportdialog_intro.ui
    resources/migration/dirresourceimportdialog_type.ui
)

qt_add_dbus_adaptor(kalarm_bin_SRCS data/org.kde.kalarm.kalarm.xml dbushandler.h DBusHandler)

qt_add_dbus_interfaces(kalarm_bin_SRCS data/org.kde.kmail.kmail.xml)

qt_add_dbus_interface(kalarm_bin_SRCS data/org.freedesktop.Notifications.xml notifications_interface)
qt_add_dbus_interface(kalarm_bin_SRCS data/org.freedesktop.DBus.Properties.xml dbusproperties)
qt_add_dbus_interface(kalarm_bin_SRCS data/org.freedesktop.ScreenSaver.xml screensaver)

kconfig_add_kcfg_files(kalarm_bin_SRCS GENERATE_MOC data/kalarmconfig.kcfgc)

#if (UNIX)
file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/appicons/*-apps-kalarm.png")
ecm_add_app_icon(kalarm_bin_SRCS ICONS ${ICONS_SRCS})
add_executable(kalarm_bin ${kalarm_bin_SRCS})

set_target_properties(kalarm_bin PROPERTIES OUTPUT_NAME kalarm)
if (COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(kalarm_bin PROPERTIES UNITY_BUILD ON)
endif()

target_compile_definitions(kalarm_bin PRIVATE -DVERSION="${KALARM_VERSION}")


target_link_libraries(kalarm_bin
    kalarmcalendar
    kalarmplugin
    KF${KF_MAJOR_VERSION}::Codecs
    KF${KF_MAJOR_VERSION}::ConfigCore
    KF${KF_MAJOR_VERSION}::Completion
    KF${KF_MAJOR_VERSION}::DBusAddons
    KF${KF_MAJOR_VERSION}::GlobalAccel
    KF${KF_MAJOR_VERSION}::GuiAddons
    KF${KF_MAJOR_VERSION}::Holidays
    KF${KF_MAJOR_VERSION}::IdleTime
    KF${KF_MAJOR_VERSION}::ItemModels
    KF${KF_MAJOR_VERSION}::KIOWidgets
    KF${KF_MAJOR_VERSION}::Notifications
    KF${KF_MAJOR_VERSION}::NotifyConfig
    KF${KF_MAJOR_VERSION}::TextWidgets
    KF${KF_MAJOR_VERSION}::WindowSystem
    KF${KF_MAJOR_VERSION}::XmlGui
    KF${KF_MAJOR_VERSION}::KIOFileWidgets
    KF${KF_MAJOR_VERSION}::Crash
    Phonon::phonon4qt${QT_MAJOR_VERSION}
    KF${KF_MAJOR_VERSION}::CalendarCore
    KPim${KF_MAJOR_VERSION}::CalendarUtils
    KF${KF_MAJOR_VERSION}::Contacts
    KPim${KF_MAJOR_VERSION}::IdentityManagementWidgets
    KF5::PimTextEdit
    KPim${KF_MAJOR_VERSION}::Mime
)
if (QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kalarm_bin Qt6::Core5Compat)
endif()
target_link_libraries(kalarm_bin KF${KF_MAJOR_VERSION}::TextEditTextToSpeech)

if (ENABLE_WAKE_FROM_SUSPEND)
	target_link_libraries(kalarm_bin KF5::AuthCore)
endif()

if (ENABLE_X11)
    if (QT_MAJOR_VERSION STREQUAL "5")
        target_link_libraries(kalarm_bin Qt::X11Extras ${X11_X11_LIB})
    elseif (Qt6_VERSION_MINOR LESS 2)
        target_link_libraries(kalarm_bin Qt6::GuiPrivate ${X11_X11_LIB})
    else()
        target_link_libraries(kalarm_bin ${X11_X11_LIB})
    endif()
endif()

install(TARGETS kalarm_bin ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
#endif (UNIX)

########### install files ###############

install(FILES data/org.kde.kalarm.desktop  DESTINATION ${KDE_INSTALL_APPDIR})
install(FILES data/kalarm.autostart.desktop DESTINATION ${KDE_INSTALL_AUTOSTARTDIR})
install(FILES data/org.kde.kalarm.appdata.xml DESTINATION ${KDE_INSTALL_METAINFODIR})

install(FILES data/kalarmconfig.kcfg DESTINATION ${KDE_INSTALL_KCFGDIR})
install(FILES data/kalarmui.rc DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/kalarm)
install(FILES data/org.kde.kalarm.kalarm.xml DESTINATION ${KDE_INSTALL_DBUSINTERFACEDIR})
install(FILES data/kalarm.notifyrc DESTINATION ${KDE_INSTALL_KNOTIFYRCDIR})

########### KAuth helper ###############

if (ENABLE_WAKE_FROM_SUSPEND)
    add_executable(kalarm_helper rtcwakeaction.cpp rtcwakeaction.h ${libkalarm_common_SRCS})
    target_link_libraries(kalarm_helper KF5::AuthCore KF${KF_MAJOR_VERSION}::I18n)
    install(TARGETS kalarm_helper DESTINATION ${KAUTH_HELPER_INSTALL_DIR})
    kauth_install_helper_files(kalarm_helper org.kde.kalarm.rtcwake root)
    kauth_install_actions(org.kde.kalarm.rtcwake data/org.kde.kalarm.rtcwake.actions)
endif()
