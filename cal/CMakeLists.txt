include_directories(
	../lib
)

########### next target ###############
set(kalarm_calendar_LIB_SRCS
	kcalendar.cpp
	karecurrence.cpp
	alarmtext.cpp
	identities.cpp
	kaeventdata.cpp
	../lib/datetime.cpp
	../lib/kalocale.cpp
)

kde4_add_library(kalarm_calendar SHARED ${kalarm_calendar_LIB_SRCS})

target_link_libraries(kalarm_calendar ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS})
target_link_libraries(kalarm_calendar LINK_INTERFACE_LIBRARIES ${KDEPIMLIBS_KCAL_LIBS} ${KDEPIMLIBS_KPIMIDENTITIES_LIBS})

set_target_properties(kalarm_calendar PROPERTIES 
   VERSION ${GENERIC_LIB_VERSION} 
   SOVERSION ${GENERIC_LIB_SOVERSION} 
)
install(TARGETS kalarm_calendar ${INSTALL_TARGETS_DEFAULT_ARGS})
