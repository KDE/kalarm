########### files copied from kdepim-runtime module ###############
set(AKONADI_SINGLEFILERESOURCE_SHARED_SOURCES
    ${AKONADI_SHARED_RESOURCES}/singlefileresourcebase.cpp
    ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigwidget.cpp
    ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigdialogbase.cpp
)

if (KDEPIM_MOBILE_UI)
set(AKONADI_SINGLEFILERESOURCE_SHARED_UI
    ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigwidget_mobile.ui
    ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigdialog.ui
)
else (KDEPIM_MOBILE_UI)
set(AKONADI_SINGLEFILERESOURCE_SHARED_UI
    ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigwidget_desktop.ui
    ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigdialog.ui
)
endif (KDEPIM_MOBILE_UI)
########### end files copied from kdepim-runtime module ###############

include_directories(
    ${KALARM_COMMON_RESOURCES}
    ${KALARM_CALENDAR_INCLUDE_DIR}
    ${AKONADI_ICAL_RESOURCE}
    ${AKONADI_SHARED_RESOURCES}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

########### next target ###############
remove_definitions(-DKDE_DEFAULT_DEBUG_AREA=5950)
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5952)
add_definitions(-DSETTINGS_NAMESPACE=Akonadi_KAlarm_Resource)

set(kalarmresource_SRCS
    ${AKONADI_SINGLEFILERESOURCE_SHARED_SOURCES}
    ${AKONADI_ICAL_RESOURCE}/icalresourcebase.cpp
    kalarmresource.cpp
    ../common/kalarmresourcecommon.cpp
)

install(FILES kalarmresource.desktop kalarmresource_active.desktop kalarmresource_archived.desktop kalarmresource_template.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents")

kde4_add_ui_files(kalarmresource_SRCS ${AKONADI_SINGLEFILERESOURCE_SHARED_UI})
kde4_add_kcfg_files(kalarmresource_SRCS settings.kcfgc)
kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kalarmresource.kcfg org.kde.Akonadi.KAlarm.Settings)
qt4_add_dbus_adaptor(kalarmresource_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarm.Settings.xml settings.h Akonadi_KAlarm_Resource::Settings icalsettingsadaptor ICalSettingsAdaptor
)

if (NOT WINCE)
    kde4_add_plugin(akonadi_kalarm_resource ${kalarmresource_SRCS})
else(NOT WINCE)
    kde4_add_plugin(akonadi_kalarm_resource STATIC ${kalarmresource_SRCS})
endif(NOT WINCE)

if (Q_WS_MAC)
    set_target_properties(akonadi_kalarm_resource PROPERTIES MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/../Info.plist.template)
    set_target_properties(akonadi_kalarm_resource PROPERTIES MACOSX_BUNDLE_GUI_IDENTIFIER "org.kde.Akonadi.KAlarm")
    set_target_properties(akonadi_kalarm_resource PROPERTIES MACOSX_BUNDLE_BUNDLE_NAME "KDE Akonadi KAlarm Resource")
endif (Q_WS_MAC)

target_link_libraries(akonadi_kalarm_resource kalarm_calendar akonadi-kcal ${KDEPIMLIBS_AKONADI_LIBS} ${QT_QTDBUS_LIBRARY} ${KDEPIMLIBS_KCALCORE_LIBS} ${KDE4_KIO_LIBS})

install(TARGETS akonadi_kalarm_resource DESTINATION ${PLUGIN_INSTALL_DIR})
