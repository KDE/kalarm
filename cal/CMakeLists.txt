########### next target ###############
set(kalarm_calendar_LIB_SRCS
	kacalendar.cpp
	karecurrence.cpp
	alarmtext.cpp
	identities.cpp
	kaevent.cpp
	datetime.cpp
	version.cpp
)
if(KALARM_USE_AKONADI)
    set(kalarm_calendar_LIB_SRCS ${kalarm_calendar_LIB_SRCS}
	collectionattribute.cpp
	eventattribute.cpp
    )
endif(KALARM_USE_AKONADI)

kde4_add_library(kalarm_calendar ${LIBRARY_TYPE} ${kalarm_calendar_LIB_SRCS})

if(KALARM_USE_AKONADI)
  target_link_libraries(kalarm_calendar ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_KCALCORE_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
  target_link_libraries(kalarm_calendar LINK_INTERFACE_LIBRARIES ${KDEPIMLIBS_AKONADI_LIBS} ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
else(KALARM_USE_AKONADI)
  target_link_libraries(kalarm_calendar ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
  target_link_libraries(kalarm_calendar LINK_INTERFACE_LIBRARIES ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
endif(KALARM_USE_AKONADI)

set_target_properties(kalarm_calendar PROPERTIES 
   VERSION ${GENERIC_LIB_VERSION} 
   SOVERSION ${GENERIC_LIB_SOVERSION} 
)
install(TARGETS kalarm_calendar ${INSTALL_TARGETS_DEFAULT_ARGS})
