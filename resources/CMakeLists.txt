########### next target ###############
set(kalarm_resources_LIB_SRCS
        alarmresource.cpp 
        alarmresources.cpp 
        resourcelocal.cpp 
        resourcelocalwidget.cpp 
        resourcelocaldir.cpp 
        resourcelocaldirwidget.cpp 
        resourceremote.cpp 
        resourceremotewidget.cpp 
        resourcewidget.cpp 
)


kde4_add_library(kalarm_resources ${LIBRARY_TYPE} ${kalarm_resources_LIB_SRCS})

target_link_libraries(kalarm_resources
                      ${KDEPIMLIBS_KALARMCAL_LIBS}
                      ${KDEPIMLIBS_KRESOURCES_LIBS}
                      ${KDEPIMLIBS_KABC_LIBS}
                      ${KDEPIMLIBS_KCAL_LIBS}
                      ${KDE4_KIO_LIBS}
                     )
target_link_libraries(kalarm_resources LINK_INTERFACE_LIBRARIES
                      ${KDEPIMLIBS_KALARMCAL_LIBS}
                      ${KDEPIMLIBS_KRESOURCES_LIBS}
                      ${KDEPIMLIBS_KABC_LIBS}
                      ${KDEPIMLIBS_KCAL_LIBS}
                      ${KDE4_KIO_LIBS}
                     )

set_target_properties(kalarm_resources PROPERTIES 
   VERSION ${KDEPIM_LIB_VERSION} 
   SOVERSION ${KDEPIM_LIB_SOVERSION} 
)
install(TARGETS kalarm_resources ${INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)

install(FILES kalarm_manager.desktop DESTINATION ${SERVICES_INSTALL_DIR}/kresources)


########### next target ###############

set(kalarm_local_PART_SRCS resourcelocal_plugin.cpp)


kde4_add_plugin(kalarm_local ${kalarm_local_PART_SRCS})

target_link_libraries(kalarm_local kalarm_resources ${KDEPIMLIBS_KCAL_LIBS} ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kalarm_local  DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES local.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kresources/alarms)

########### next target ###############

set(kalarm_localdir_PART_SRCS resourcelocaldir_plugin.cpp)


kde4_add_plugin(kalarm_localdir ${kalarm_localdir_PART_SRCS})

target_link_libraries(kalarm_localdir kalarm_resources ${KDEPIMLIBS_KCAL_LIBS} ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kalarm_localdir  DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES localdir.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kresources/alarms)

########### next target ###############

set(kalarm_remote_PART_SRCS resourceremote_plugin.cpp)


kde4_add_plugin(kalarm_remote ${kalarm_remote_PART_SRCS})

target_link_libraries(kalarm_remote kalarm_resources ${KDEPIMLIBS_KCAL_LIBS} ${KDE4_KDECORE_LIBS} ${QT_QTGUI_LIBRARY})

install(TARGETS kalarm_remote  DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES remote.desktop  DESTINATION ${SERVICES_INSTALL_DIR}/kresources/alarms)

