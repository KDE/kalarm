project(kalarmcal)

# The library version should be the same as that returned by
# KAEvent::currentCalendarVersion()  (see kaevent.cpp)
set(LIB_VERSION 2.7.0)
set(LIB_SOVERSION 2)

add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5955)

############### Build Options ###############

# KALARM_USE_KRESOURCES - builds to use KResources instead of Akonadi
option(KALARM_USE_KRESOURCES "Build to use KResources" OFF)

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
if(NOT KALARM_USE_KRESOURCES)
    set(kalarmcal_LIB_SRCS ${kalarmcal_LIB_SRCS}
	collectionattribute.cpp
	compatibilityattribute.cpp
	eventattribute.cpp
       )
endif(NOT KALARM_USE_KRESOURCES)

kde4_add_library(kalarmcal ${LIBRARY_TYPE} ${kalarmcal_LIB_SRCS})

if(KALARM_USE_KRESOURCES)
    target_link_libraries(kalarmcal
		          ${KDEPIMLIBS_KCAL_LIBS}
			  ${KDEPIMLIBS_KPIMIDENTITIES_LIBS}
			  ${KDEPIMLIBS_KHOLIDAYS_LIBS}
			 )
else(KALARM_USE_KRESOURCES)
    target_link_libraries(kalarmcal
		          ${KDEPIMLIBS_AKONADI_LIBS}
			  ${KDEPIMLIBS_KCALCORE_LIBS}
			  ${KDEPIMLIBS_KPIMIDENTITIES_LIBS}
			  ${KDEPIMLIBS_KHOLIDAYS_LIBS}
			 )
endif(KALARM_USE_KRESOURCES)

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
	kalarmcal_export.h
	karecurrence.h
	kcalcore_constptr.h
	repetition.h
	version.h
	DESTINATION ${INCLUDE_INSTALL_DIR}/kalarmcal
	COMPONENT Devel
)
if(NOT KALARM_USE_KRESOURCES)
    install(FILES
	    collectionattribute.h
	    compatibilityattribute.h
	    eventattribute.h
	    DESTINATION ${INCLUDE_INSTALL_DIR}/kalarmcal
	    COMPONENT Devel
           )
endif(NOT KALARM_USE_KRESOURCES)
