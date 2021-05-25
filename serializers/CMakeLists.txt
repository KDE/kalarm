add_definitions(-DTRANSLATION_DOMAIN=\"libkalarmcal5-serializer\")

add_library(akonadi_serializer_kalarm MODULE)
target_sources(akonadi_serializer_kalarm PRIVATE
    akonadi_serializer_kalarm.cpp
    kaeventformatter.cpp
)

ecm_qt_declare_logging_category(akonadi_serializer_kalarm
        HEADER akonadi_serializer_kalarm_debug.h
        IDENTIFIER AKONADI_SERIALIZER_KALARM_LOG
        CATEGORY_NAME org.kde.pim.akonadi_serializer_kalarm
        DESCRIPTION "kalarmcal (akonadi_serializer_kalarmcal)"
        EXPORT KALARMCAL
    )



if (COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(akonadi_serializer_kalarm PROPERTIES UNITY_BUILD ON)
endif()
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
