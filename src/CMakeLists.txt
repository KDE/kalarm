
add_definitions( -DQT_NO_CAST_FROM_ASCII )
add_definitions( -DQT_NO_CAST_TO_ASCII )

########### next target ###############
set(kalarmcal_LIB_SRCS
    kacalendar.cpp
    karecurrence.cpp
    alarmtext.cpp
    identities.cpp
    kaevent.cpp
    datetime.cpp
    repetition.cpp
    version.cpp
    collectionattribute.cpp
    compatibilityattribute.cpp
    eventattribute.cpp
)


add_library(KF5AlarmCalendar ${kalarmcal_LIB_SRCS})

generate_export_header(KF5AlarmCalendar BASE_NAME kalarmcal)

add_library(KF5::AlarmCalendar ALIAS KF5AlarmCalendar)

target_link_libraries(KF5AlarmCalendar
PUBLIC
                      KF5::Holidays
                      KF5::IdentityManagement
                      KF5::CoreAddons
                      KF5::CalendarCore
                      KF5::AkonadiCore
PRIVATE
)

set_target_properties(KF5AlarmCalendar PROPERTIES
      VERSION ${KALARMCAL_VERSION_STRING}
      SOVERSION ${KALARMCAL_SOVERSION}
      EXPORT_NAME AlarmCalendar
)


target_include_directories(KF5AlarmCalendar INTERFACE "$<INSTALL_INTERFACE:${KF5_INCLUDE_INSTALL_DIR}/KAlarmCal>")
target_include_directories(KF5AlarmCalendar PUBLIC "$<BUILD_INTERFACE:${KAlarmCal_SOURCE_DIR}/src;${KAlarmCal_BINARY_DIR}/src>")


install(TARGETS KF5AlarmCalendar EXPORT KF5AlarmCalendarTargets ${KF5_INSTALL_TARGETS_DEFAULT_ARGS})


ecm_generate_headers(KAlarmCal_CamelCase_HEADERS
  HEADER_NAMES
  AlarmText
  CollectionAttribute
  CompatibilityAttribute
  DateTime
  EventAttribute
  KAEvent
  KARecurrence
  Repetition
  PREFIX KAlarmCal
  REQUIRED_HEADERS KAlarmCal_HEADERS
)


install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_export.h
        identities.h
        kacalendar.h
        version.h
        ${KAlarmCal_HEADERS}
        DESTINATION ${KF5_INCLUDE_INSTALL_DIR}/KAlarmCal/kalarmcal
        COMPONENT Devel
)
install(FILES
        ${KAlarmCal_CamelCase_HEADERS}
        DESTINATION ${KF5_INCLUDE_INSTALL_DIR}/KAlarmCal/KAlarmCal/
        COMPONENT Devel
)


ecm_generate_pri_file(BASE_NAME KAlarmCal LIB_NAME KF5AlarmCal DEPS "Holidays PimIdentities CoreAddons CalendarCore AkonadiCore" FILENAME_VAR PRI_FILENAME INCLUDE_INSTALL_DIR ${KF5_INCLUDE_INSTALL_DIR}/KAlarmCal/)
install(FILES ${PRI_FILENAME} DESTINATION ${ECM_MKSPECS_INSTALL_DIR})


