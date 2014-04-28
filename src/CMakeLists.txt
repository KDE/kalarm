# The includes in akonadi/ use exceptions, we must enable them here for clang to be happy.
kde_enable_exceptions()

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5955)
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
   )

set(kalarmcal_LIB_SRCS ${kalarmcal_LIB_SRCS}
    collectionattribute.cpp
    compatibilityattribute.cpp
    eventattribute.cpp
    )


add_library(KF5KAlarmCal ${kalarmcal_LIB_SRCS})

generate_export_header(KF5KAlarmCal BASE_NAME kalarmcal)

add_library(KF5::KAlarmCal ALIAS KF5KAlarmCal)

target_link_libraries(KF5KAlarmCal
                      KF5::Holidays
                      KF5::PimIdentities
                      KF5::CoreAddons
                      KF5::CalendarCore
                      KF5::AkonadiCore
                      )

set_target_properties(KF5KAlarmCal PROPERTIES
      VERSION ${KALARMCAL_VERSION_STRING}
      SOVERSION ${KALARMCAL_SOVERSION}
      EXPORT_NAME KAlarmCal
)


target_include_directories(KF5KAlarmCal INTERFACE "$<INSTALL_INTERFACE:${KF5_INCLUDE_INSTALL_DIR}/KAlarmCal>")
target_include_directories(KF5KAlarmCal PUBLIC "$<BUILD_INTERFACE:${KAlarmCal_SOURCE_DIR}/src;${KAlarmCal_BINARY_DIR}/src>")


install(TARGETS KF5KAlarmCal EXPORT KF5KAlarmCalTargets ${INSTALL_TARGETS_DEFAULT_ARGS})


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
  REQUIRED_HEADERS KAlarmCal_HEADERS
  PREFIX KAlarmCal
)


install(FILES
        alarmtext.h
        datetime.h
        identities.h
        kacalendar.h
        kaevent.h
        ${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_export.h
        karecurrence.h
        repetition.h
        version.h
        collectionattribute.h
        compatibilityattribute.h
        eventattribute.h
        ${KAlarmCal_HEADERS}
    ${KAlarmCal_CamelCase_HEADERS}
    DESTINATION ${KF5_INCLUDE_INSTALL_DIR}/KAlarmCal
    COMPONENT Devel
)
