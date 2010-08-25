# Set location of this directory:
#   LOCAL_BUILD = TRUE if kdepim/kalarm/akonadi,
#               = FALSE if kdepim/runtime/resources/ical/alarms
set(LOCAL_BUILD TRUE)
if (LOCAL_BUILD)
    # Note that resource configuration dialogs don't appear if built with this option.
    set(KALARM_CALENDAR_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/kalarm/cal)
    set(AKONADI_ICAL_RESOURCE ${CMAKE_SOURCE_DIR}/runtime/resources/ical)

    ########### definitions copied from runtime/ ###############
    set(SHARED_MIME_INFO_MINIMUM_VERSION "0.30")
    find_package(SharedMimeInfo)
    macro_log_feature(SHARED_MIME_INFO_FOUND "SMI" "SharedMimeInfo" "http://freedesktop.org/wiki/Software/shared-mime-info" TRUE "0.30" "SharedMimeInfo is required.")

    set(AKONADI_SHARED_RESOURCES ${CMAKE_SOURCE_DIR}/runtime/resources/shared)

    set(AKONADI_SINGLEFILERESOURCE_SHARED_SOURCES
        ${AKONADI_SHARED_RESOURCES}/singlefileresourcebase.cpp
        ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigwidget.cpp
        ${AKONADI_SHARED_RESOURCES}/singlefileresourceconfigdialogbase.cpp
    )

    if (KDEPIM_MOBILE_UI)
    set(AKONADI_SINGLEFILERESOURCE_SHARED_UI
        ${CMAKE_SOURCE_DIR}/runtime/resources/shared/singlefileresourceconfigwidget_mobile.ui
        ${CMAKE_SOURCE_DIR}/runtime/resources/shared/singlefileresourceconfigdialog.ui
    )
    else (KDEPIM_MOBILE_UI)
    set(AKONADI_SINGLEFILERESOURCE_SHARED_UI
        ${CMAKE_SOURCE_DIR}/runtime/resources/shared/singlefileresourceconfigwidget_desktop.ui
        ${CMAKE_SOURCE_DIR}/runtime/resources/shared/singlefileresourceconfigdialog.ui
    )
    endif (KDEPIM_MOBILE_UI)
    ########### end definitions copied from runtime/ ###############
else (LOCAL_BUILD)
    set(KALARM_USE_AKONADI TRUE)
    set(KALARM_CALENDAR_INCLUDE_DIR cal)
    set(AKONADI_ICAL_RESOURCE ${CMAKE_CURRENT_SOURCE_DIR}/..)

    add_definitions(-DUSE_AKONADI)
    add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5950)

    add_subdirectory(cal)
endif (LOCAL_BUILD)

include_directories(
    ${KALARM_CALENDAR_INCLUDE_DIR}
    ${AKONADI_ICAL_RESOURCE}
    ${AKONADI_SHARED_RESOURCES}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")

########### next target ###############

set(kalarmresource_SRCS
    ${AKONADI_SINGLEFILERESOURCE_SHARED_SOURCES}
    ${AKONADI_ICAL_RESOURCE}/icalresourcebase.cpp
    kalarmresource.cpp
)

install(FILES kalarmresource.desktop kalarmresource_active.desktop kalarmresource_archived.desktop kalarmresource_template.desktop DESTINATION "${CMAKE_INSTALL_PREFIX}/share/akonadi/agents")

kde4_add_ui_files(kalarmresource_SRCS ${AKONADI_SINGLEFILERESOURCE_SHARED_UI})
kde4_add_kcfg_files(kalarmresource_SRCS settings.kcfgc)
kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/kalarmresource.kcfg org.kde.Akonadi.KAlarm.Settings)
qt4_add_dbus_adaptor(kalarmresource_SRCS
    ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarm.Settings.xml settings.h Settings
)

kde4_add_executable(akonadi_kalarm_resource ${kalarmresource_SRCS})

target_link_libraries(akonadi_kalarm_resource kalarm_calendar akonadi-kcal ${KDEPIMLIBS_AKONADI_LIBS} ${QT_QTDBUS_LIBRARY} ${KDEPIMLIBS_KCALCORE_LIBS} ${KDE4_KIO_LIBS})

install(TARGETS akonadi_kalarm_resource ${INSTALL_TARGETS_DEFAULT_ARGS})

########### next target ###############

set(akonadi_serializer_kalarm_SRCS
    akonadi_serializer_kalarm.cpp
)

kde4_add_plugin(akonadi_serializer_kalarm ${akonadi_serializer_kalarm_SRCS})
target_link_libraries(akonadi_serializer_kalarm kalarm_calendar ${KDEPIMLIBS_KCALCORE_LIBS} ${KDEPIMLIBS_AKONADI_LIBS} ${KDE4_KDECORE_LIBS})
install(TARGETS akonadi_serializer_kalarm DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES akonadi_serializer_kalarm.desktop DESTINATION ${DATA_INSTALL_DIR}/akonadi/plugins/serializer)

############### Here we install some extra stuff  ###############

## install the MIME type spec file for KAlarm specific MIME types
install(FILES kalarm-mime.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})
