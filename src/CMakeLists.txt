project(kalarmcal)

find_package(Qt5 NO_MODULE REQUIRED COMPONENTS Sql)
find_package(KF5 NO_MODULE REQUIRED COMPONENTS CoreAddons ItemViews)

# The library version should be the same as that returned by
# KAEvent::currentCalendarVersion()  (see kaevent.cpp)
set(LIB_VERSION 2.7.0)
set(LIB_SOVERSION 2)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5955)
add_definitions( -DQT_NO_CAST_FROM_ASCII )
add_definitions( -DQT_NO_CAST_TO_ASCII )


include_directories(
  ${kcalcore_BINARY_DIR}
)

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
    # The includes in akonadi/ use exceptions, we must enable them here for clang to be happy.
    kde_enable_exceptions()

    set(kalarmcal_LIB_SRCS ${kalarmcal_LIB_SRCS}
	collectionattribute.cpp
	compatibilityattribute.cpp
	eventattribute.cpp
       )

include_directories(
  ${CMAKE_BINARY_DIR}/kcalcore
  ${CMAKE_BINARY_DIR}/akonadi
  ${CMAKE_BINARY_DIR}/kholidays
  ${CMAKE_BINARY_DIR}/kpimidentities
)
add_library(kalarmcal ${LIBRARY_TYPE} ${kalarmcal_LIB_SRCS})
generate_export_header(kalarmcal)
file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_export.h" "\n// This library was built to use Akonadi.\n// It must be rebuilt in order to use KResources.\n#ifdef KALARMCAL_USE_KRESOURCES\n#undef KALARMCAL_USE_KRESOURCES\n#endif\n")

target_link_libraries(kalarmcal
                      kholidays
                      kpimidentities
                      KF5::CoreAddons
                      )
set_target_properties(kalarmcal PROPERTIES 
                      VERSION ${LIB_VERSION} 
                      SOVERSION ${LIB_SOVERSION} 
                     )
install(TARGETS kalarmcal EXPORT kdepimlibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})


ecm_generate_headers(KAlarmCal_CamelCase_HEADERS
  HEADER_NAMES
  AlarmText  
  CollectionAttribute  
  CompatibilityAttribute  
  DateTime  
  EventAttribute  
  KAAlarm  
  KAEvent  
  KARecurrence  
  Repetition
  REQUIRED_HEADERS KAlarmCal_HEADERS
  PREFIX KAlarmCal
)


install(FILES
    ${KAlarmCal_CamelCase_HEADERS}
    DESTINATION ${INCLUDE_INSTALL_DIR}/KAlarmCal
    COMPONENT Devel
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
        ${KAlarmCal_HEADERS}
	DESTINATION ${INCLUDE_INSTALL_DIR}/kalarmcal
	COMPONENT Devel
)
install(FILES
	    collectionattribute.h
	    compatibilityattribute.h
	    eventattribute.h
	    DESTINATION ${INCLUDE_INSTALL_DIR}/kalarmcal
	    COMPONENT Devel
           )
