############### Build Options ###############

# KALARM_USE_KRESOURCES - builds to use KResources instead of Akonadi
#option(KALARM_USE_KRESOURCES "Build to use KResources" OFF)

if(NOT KALARM_USE_AKONADI)
    add_definitions(-DUSE_KRESOURCES)
endif(NOT KALARM_USE_AKONADI)

########### next target ###############
set(kalarm_calendar_LIB_SRCS
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
    set(kalarm_calendar_LIB_SRCS ${kalarm_calendar_LIB_SRCS}
	collectionattribute.cpp
	compatibilityattribute.cpp
	eventattribute.cpp
    )
endif(NOT KALARM_USE_KRESOURCES)

kde4_add_library(kalarm_calendar ${LIBRARY_TYPE} ${kalarm_calendar_LIB_SRCS})

if(KALARM_USE_KRESOURCES)
    target_link_libraries(kalarm_calendar ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
    target_link_libraries(kalarm_calendar LINK_INTERFACE_LIBRARIES ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
else(KALARM_USE_KRESOURCES)
    target_link_libraries(kalarm_calendar ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_KCALCORE_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
    target_link_libraries(kalarm_calendar LINK_INTERFACE_LIBRARIES ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
endif(KALARM_USE_KRESOURCES)

set_target_properties(kalarm_calendar PROPERTIES 
    VERSION ${GENERIC_LIB_VERSION} 
    SOVERSION ${GENERIC_LIB_SOVERSION} 
)
install(TARGETS kalarm_calendar ${INSTALL_TARGETS_DEFAULT_ARGS})
