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

############### Build Options ###############

# KALARM_USE_KRESOURCES - builds to use KResources instead of Akonadi
option(KALARM_USE_KRESOURCES "Build to use KResources" OFF)

set(USE_KRESOURCES false)
if(KALARM_USE_KRESOURCES AND NOT KDEPIM_NO_KRESOURCES)
    set(USE_KRESOURCES true)
    add_definitions(
      -DKRESOURCES_DEPRECATED=
      -DKCAL_DEPRECATED=
    )
    # This one won't be needed when CMake 2.8.13 is depended on.
    add_definitions(
      -DKRESOURCES_DEPRECATED_EXPORT=KRESOURCES_EXPORT
      -DKCAL_DEPRECATED_EXPORT=KCAL_EXPORT
    )
endif()

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
if(NOT USE_KRESOURCES)
    # The includes in akonadi/ use exceptions, we must enable them here for clang to be happy.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")

    set(kalarmcal_LIB_SRCS ${kalarmcal_LIB_SRCS}
	collectionattribute.cpp
	compatibilityattribute.cpp
	eventattribute.cpp
       )
endif()

include_directories(
  ${CMAKE_BINARY_DIR}/kcal
  ${CMAKE_BINARY_DIR}/kcalcore
  ${CMAKE_BINARY_DIR}/akonadi
  ${CMAKE_BINARY_DIR}/kholidays
  ${CMAKE_BINARY_DIR}/kpimidentities
)
add_library(kalarmcal ${LIBRARY_TYPE} ${kalarmcal_LIB_SRCS})
generate_export_header(kalarmcal)
if(USE_KRESOURCES)
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_export.h" "\n// This library was built to use KResources.\n// It must be rebuilt in order to use Akonadi.\n#define KALARMCAL_USE_KRESOURCES\n")
else()
    file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_export.h" "\n// This library was built to use Akonadi.\n// It must be rebuilt in order to use KResources.\n#ifdef KALARMCAL_USE_KRESOURCES\n#undef KALARMCAL_USE_KRESOURCES\n#endif\n")
endif()

if(USE_KRESOURCES)
    target_link_libraries(kalarmcal
                          kcal
                          kholidays
                          kpimidentities
                          KF5::CoreAddons
                         )
else()
    target_link_libraries(kalarmcal
                          akonadi-kde
                          kcalcore
                          kholidays
                          kpimidentities
                          KF5::CoreAddons
                         )
endif()

set_target_properties(kalarmcal PROPERTIES 
                      VERSION ${LIB_VERSION} 
                      SOVERSION ${LIB_SOVERSION} 
                     )
install(TARGETS kalarmcal EXPORT kdepimlibsLibraryTargets ${INSTALL_TARGETS_DEFAULT_ARGS})

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
	DESTINATION ${INCLUDE_INSTALL_DIR}/kalarmcal
	COMPONENT Devel
)
if(NOT USE_KRESOURCES)
    install(FILES
	    collectionattribute.h
	    compatibilityattribute.h
	    eventattribute.h
	    DESTINATION ${INCLUDE_INSTALL_DIR}/kalarmcal
	    COMPONENT Devel
           )
endif()
