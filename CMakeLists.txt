project(kalarmcal)

# The library version should be the same as that returned by
# KAEvent::currentCalendarVersion()  (see kaevent.cpp)
set(LIB_VERSION 2.7.0)
set(LIB_SOVERSION 2)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5955)

include_directories(
  ${kcalcore_BINARY_DIR}
)

############### Build Options ###############

# KALARM_USE_KRESOURCES - builds to use KResources instead of Akonadi
option(KALARM_USE_KRESOURCES "Build to use KResources" OFF)

set(USE_KRESOURCES false)
if(KALARM_USE_KRESOURCES AND NOT KDEPIM_NO_KRESOURCES)
    set(USE_KRESOURCES true)
    add_definitions(-DWANT_DEPRECATED_KCAL_API)
    add_definitions(-DWANT_DEPRECATED_KRESOURCE_API)
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
    set(kalarmcal_LIB_SRCS ${kalarmcal_LIB_SRCS}
	collectionattribute.cpp
	compatibilityattribute.cpp
	eventattribute.cpp
       )
endif()

if(USE_KRESOURCES)
    set(KALARMCAL_USE_KRESOURCES "// This library was built to use KResources.\n// It must be rebuilt in order to use Akonadi.\n#define KALARMCAL_USE_KRESOURCES")
else()
    set(KALARMCAL_USE_KRESOURCES "// This library was built to use Akonadi.\n// It must be rebuilt in order to use KResources.\n#ifdef KALARMCAL_USE_KRESOURCES\n#undef KALARMCAL_USE_KRESOURCES\n#endif")
endif()

# NOTE: kalarmcal_export.h is only generated when cmake is run.
#       If it is deleted, 'make' will not regenerate it.
configure_file(kalarmcal_export.h.in ${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_export.h)

kde4_add_library(kalarmcal ${LIBRARY_TYPE} ${kalarmcal_LIB_SRCS})

if(USE_KRESOURCES)
    target_link_libraries(kalarmcal
                          kcal
                          kholidays
                          kpimidentities
                          ${KDE4_KDECORE_LIBS}
                          ${QT_QTGUI_LIBRARY}
                          ${QT_QTCORE_LIBRARY}
                         )
else()
    target_link_libraries(kalarmcal
                          akonadi-kde
                          kcalcore
                          kholidays
                          kpimidentities
                          ${KDE4_KDECORE_LIBS}
                          ${QT_QTGUI_LIBRARY}
                          ${QT_QTCORE_LIBRARY}
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
