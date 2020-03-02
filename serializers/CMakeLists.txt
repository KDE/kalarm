add_definitions(-DTRANSLATION_DOMAIN=\"libkalarmcal5-serializer\")

set(akonadi_serializer_kalarm_SRCS
    akonadi_serializer_kalarm.cpp
    kaeventformatter.cpp
)

if (ECM_VERSION VERSION_LESS "5.68.0")
    ecm_qt_declare_logging_category(akonadi_serializer_kalarm_SRCS
        HEADER akonadi_serializer_kalarm_debug.h
        IDENTIFIER AKONADI_SERIALIZER_KALARM_LOG
        CATEGORY_NAME org.kde.pim.akonadi_serializer_kalarm
    )
else()
    ecm_qt_declare_logging_category(akonadi_serializer_kalarm_SRCS
        HEADER akonadi_serializer_kalarm_debug.h
        IDENTIFIER AKONADI_SERIALIZER_KALARM_LOG
        CATEGORY_NAME org.kde.pim.akonadi_serializer_kalarm
        DESCRIPTION "kalarmcal (akonadi_serializer_kalarmcal)"
        EXPORT KALARMCAL
    )
endif()


add_library(akonadi_serializer_kalarm MODULE ${akonadi_serializer_kalarm_SRCS})
target_link_libraries(akonadi_serializer_kalarm
    KF5::AlarmCalendar
    KF5::CalendarCore
    KF5::CalendarUtils
    KF5::AkonadiCore
)

install(TARGETS akonadi_serializer_kalarm
    DESTINATION ${KDE_INSTALL_PLUGINDIR}
)
install(FILES akonadi_serializer_kalarm.desktop
    DESTINATION ${KDE_INSTALL_DATADIR}/akonadi/plugins/serializer
)
