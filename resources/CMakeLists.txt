
#include_directories( ${CMAKE_SOURCE_DIR}/kalarm ${KDE4_INCLUDE_DIR} ${QT_INCLUDES} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}  )


########### next target ###############
set(kalarm_resources_LIB_SRCS
	alarmresource.cpp 
	alarmresources.cpp 
	kcal.cpp 
	resourcelocal.cpp 
	resourcelocal_plugin.cpp 
	resourcelocalwidget.cpp 
	resourcelocaldir.cpp 
	resourcelocaldir_plugin.cpp 
	resourcelocaldirwidget.cpp 
	resourceremote.cpp 
	resourceremote_plugin.cpp 
	resourceremotewidget.cpp 
	resourcewidget.cpp 
)

kde4_automoc(${kalarm_resources_LIB_SRCS})

kde4_add_library(kalarm_resources SHARED ${kalarm_resources_LIB_SRCS})

target_link_libraries(kalarm_resources ${KDE4_KDECORE_LIBS} kcal )

set_target_properties(kalarm_resources PROPERTIES VERSION 1.0.0 SOVERSION 1 )
install_targets(${LIB_INSTALL_DIR} kalarm_resources )


########### next target ###############

set(kalarm_local_PART_SRCS resourcelocal_plugin.cpp )

kde4_automoc(${kalarm_local_PART_SRCS})

kde4_add_plugin(kalarm_local ${kalarm_local_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kalarm_local )

target_link_libraries(kalarm_local ${KDE4_KDECORE_LIBS} kalarm_resources )

install_targets(${PLUGIN_INSTALL_DIR} kalarm_local )

install_files( ${SERVICES_INSTALL_DIR}/kresources/alarms FILES local.desktop )

########### next target ###############

set(kalarm_localdir_PART_SRCS resourcelocaldir_plugin.cpp )

kde4_automoc(${kalarm_localdir_PART_SRCS})

kde4_add_plugin(kalarm_localdir ${kalarm_localdir_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kalarm_localdir )

target_link_libraries(kalarm_localdir ${KDE4_KDECORE_LIBS} kalarm_resources )

install_targets(${PLUGIN_INSTALL_DIR} kalarm_localdir )

install_files( ${SERVICES_INSTALL_DIR}/kresources/alarms FILES localdir.desktop )

########### next target ###############

set(kalarm_remote_PART_SRCS resourceremote_plugin.cpp )

kde4_automoc(${kalarm_remote_PART_SRCS})

kde4_add_plugin(kalarm_remote ${kalarm_remote_PART_SRCS})

kde4_install_libtool_file( ${PLUGIN_INSTALL_DIR} kalarm_remote )

target_link_libraries(kalarm_remote ${KDE4_KDECORE_LIBS} kalarm_resources )

install_targets(${PLUGIN_INSTALL_DIR} kalarm_remote )

install_files( ${SERVICES_INSTALL_DIR}/kresources/alarms FILES remote.desktop )

