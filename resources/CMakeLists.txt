
########### next target ###############
set(kalarm_resources_LIB_SRCS
	alarmresource.cpp 
	alarmresources.cpp 
	kcalendar.cpp 
	resourcelocal.cpp 
	resourcelocalwidget.cpp 
	resourcelocaldir.cpp 
	resourcelocaldirwidget.cpp 
	resourceremote.cpp 
	resourceremotewidget.cpp 
	resourcewidget.cpp 
)


kde4_add_library(kalarm_resources SHARED ${kalarm_resources_LIB_SRCS})

target_link_libraries(kalarm_resources ${KDE4_KIO_LIBS} ${KDE4_KRESOURCES_LIBS} ${KDE4_KABC_LIBS} ${KDE4_KCAL_LIBS})

set_target_properties(kalarm_resources PROPERTIES 
   VERSION ${GENERIC_LIB_VERSION} 
   SOVERSION ${GENERIC_LIB_SOVERSION} 
   ${KDE4_DISABLE_PROPERTY_}LINK_INTERFACE_LIBRARIES "kresources"
)
install(TARGETS kalarm_resources DESTINATION ${LIB_INSTALL_DIR})

install(FILES kalarm_manager.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kresources)


########### next target ###############

set(kalarm_local_PART_SRCS resourcelocal_plugin.cpp )


kde4_add_plugin(kalarm_local ${kalarm_local_PART_SRCS})

target_link_libraries(kalarm_local ${KDE4_KDECORE_LIBS} kalarm_resources )

install(TARGETS kalarm_local  DESTINATION ${PLUGIN_INSTALL_DIR})

install( FILES local.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kresources/alarms)

########### next target ###############

set(kalarm_localdir_PART_SRCS resourcelocaldir_plugin.cpp )


kde4_add_plugin(kalarm_localdir ${kalarm_localdir_PART_SRCS})

target_link_libraries(kalarm_localdir ${KDE4_KDECORE_LIBS} kalarm_resources )

install(TARGETS kalarm_localdir  DESTINATION ${PLUGIN_INSTALL_DIR})

install( FILES localdir.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kresources/alarms)

########### next target ###############

set(kalarm_remote_PART_SRCS resourceremote_plugin.cpp )


kde4_add_plugin(kalarm_remote ${kalarm_remote_PART_SRCS})

target_link_libraries(kalarm_remote ${KDE4_KDECORE_LIBS} kalarm_resources )

install(TARGETS kalarm_remote  DESTINATION ${PLUGIN_INSTALL_DIR})

install( FILES remote.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kresources/alarms)

