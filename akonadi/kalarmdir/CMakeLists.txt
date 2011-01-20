include_directories(
    ${KALARM_COMMON_RESOURCES}
    ${KALARM_CALENDAR_INCLUDE_DIR}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

########### next target ###############
remove_definitions(-DKDE_DEFAULT_DEBUG_AREA=5950)
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5953)
add_definitions(-DSETTINGS_NAMESPACE=Akonadi_KAlarm_Dir_Resource)

set(kalarmdirresource_SRCS
    settingsdialog.cpp
    kalarmdirresource.cpp
    ../common/kalarmresourcecommon.cpp
    ../common/alarmtypewidget.cpp
)

install(FILES kalarmdirresource.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents")

kde4_add_ui_files(kalarmdirresource_SRCS settingsdialog.ui ../common/alarmtypewidget.ui)
kde4_add_kcfg_files(kalarmdirresource_SRCS settings.kcfgc)
kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kalarmdirresource.kcfg org.kde.Akonadi.KAlarmDir.Settings)
qt4_add_dbus_adaptor(kalarmdirresource_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarmDir.Settings.xml settings.h Akonadi_KAlarm_Dir_Resource::Settings kalarmdirsettingsadaptor KAlarmDirSettingsAdaptor
)

if (NOT WINCE)
    kde4_add_plugin(akonadi_kalarm_dir_resource ${kalarmdirresource_SRCS})
else(NOT WINCE)
    kde4_add_plugin(akonadi_kalarm_dir_resource STATIC ${kalarmdirresource_SRCS})
endif(NOT WINCE)

if (Q_WS_MAC)
    set_target_properties(akonadi_kalarm_dir_resource PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist.template)
    set_target_properties(akonadi_kalarm_dir_resource PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.Akonadi.KAlarmDir")
    set_target_properties(akonadi_kalarm_dir_resource PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "KDE Akonadi KAlarm Directory Resource")
endif (Q_WS_MAC)

target_link_libraries(akonadi_kalarm_dir_resource kalarm_calendar akonadi-kcal ${KDEPIMLIBS_AKONADI_LIBS} ${QT_QTDBUS_LIBRARY} ${KDEPIMLIBS_KCALCORE_LIBS} ${KDE4_KIO_LIBS})

install(TARGETS akonadi_kalarm_dir_resource DESTINATION ${PLUGIN_INSTALL_DIR})
