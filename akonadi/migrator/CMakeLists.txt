add_definitions(-DWANT_DEPRECATED_KRESOURCE_API)

set(kalarm-migrator_SRCS
    main.cpp
    alarmmigrator.cpp
)

kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/../kalarm/kalarmresource.kcfg org.kde.Akonadi.KAlarm.Settings)
qt4_add_dbus_interface(kalarm-migrator_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarm.Settings.xml kalarmsettings KAlarmSettings)

kcfg_generate_dbus_interface(${CMAKE_CURRENT_SOURCE_DIR}/../kalarmdir/kalarmdirresource.kcfg org.kde.Akonadi.KAlarmDir.Settings)
qt4_add_dbus_interface(kalarm-migrator_SRCS ${CMAKE_CURRENT_BINARY_DIR}/org.kde.Akonadi.KAlarmDir.Settings.xml kalarmdirsettings KAlarmDirSettings)

kde4_add_app_icon(kalarm-migrator_SRCS "${KDE4_ICON_DIR}/oxygen/*/apps/akonadi.png")
kde4_add_executable(kalarm-migrator ${kalarm-migrator_SRCS})

target_link_libraries(kalarm-migrator
    kalarm_calendar
    ${KDEPIMLIBS_AKONADI_LIBS}
    ${KDEPIMLIBS_KRESOURCES_LIBS}
    ${KDE4_KDECORE_LIBS}
)

install(TARGETS kalarm-migrator ${INSTALL_TARGETS_DEFAULT_ARGS})
