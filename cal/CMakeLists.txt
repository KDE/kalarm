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

kde4_add_library(kalarm_calendar SHARED ${kalarm_calendar_LIB_SRCS})

target_link_libraries(kalarm_calendar ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})
target_link_libraries(kalarm_calendar LINK_INTERFACE_LIBRARIES ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS} ${KDEPIMLIBS_KHOLIDAYS_LIBS})

set_target_properties(kalarm_calendar PROPERTIES 
   VERSION ${GENERIC_LIB_VERSION} 
   SOVERSION ${GENERIC_LIB_SOVERSION} 
)
install(TARGETS kalarm_calendar ${INSTALL_TARGETS_DEFAULT_ARGS})
