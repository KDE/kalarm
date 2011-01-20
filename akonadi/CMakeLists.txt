# Note that resource configuration dialogs don't appear if built with this option.
set(KALARM_CALENDAR_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/kalarm/cal)

########### definitions copied from kdepim-runtime module ###############
set(SHARED_MIME_INFO_MINIMUM_VERSION "0.30")
find_package(SharedMimeInfo)
macro_log_feature(SHARED_MIME_INFO_FOUND "SMI" "SharedMimeInfo" "http://freedesktop.org/wiki/Software/shared-mime-info" TRUE "0.30" "SharedMimeInfo is required.")

if (RUNTIME_PLUGINS_STATIC)
    set(SERIALIZER_TYPE STATIC)
endif (RUNTIME_PLUGINS_STATIC)

set(AKONADI_ICAL_RESOURCE ${CMAKE_CURRENT_SOURCE_DIR}/kdepim-runtime/ical)
set(AKONADI_SHARED_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/kdepim-runtime/shared)
########### end definitions copied from kdepim-runtime module ###############

set(KALARM_COMMON_RESOURCES ${CMAKE_CURRENT_SOURCE_DIR}/common)

add_subdirectory(kalarm)
add_subdirectory(kalarmdir)

include_directories(
    ${KALARM_CALENDAR_INCLUDE_DIR}
#    ${AKONADI_ICAL_RESOURCE}
#    ${AKONADI_SHARED_RESOURCES}
    ${QT_QTDBUS_INCLUDE_DIR}
    ${Boost_INCLUDE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${KDE4_ENABLE_EXCEPTIONS}")

########### next target ###############
remove_definitions(-DKDE_DEFAULT_DEBUG_AREA=5950)
add_definitions(-DKDE_DEFAULT_DEBUG_AREA=5954)

set(akonadi_serializer_kalarm_SRCS
    akonadi_serializer_kalarm.cpp
    kaeventformatter.cpp
)

kde4_add_plugin(akonadi_serializer_kalarm ${SERIALIZER_TYPE} ${akonadi_serializer_kalarm_SRCS})
target_link_libraries(akonadi_serializer_kalarm kalarm_calendar ${KDEPIMLIBS_KCALCORE_LIBS} ${KDEPIMLIBS_KCALUTILS_LIBS} ${KDEPIMLIBS_AKONADI_LIBS} ${KDE4_KDECORE_LIBS})
install(TARGETS akonadi_serializer_kalarm DESTINATION ${PLUGIN_INSTALL_DIR})

install(FILES akonadi_serializer_kalarm.desktop DESTINATION ${DATA_INSTALL_DIR}/akonadi/plugins/serializer)

############### Here we install some extra stuff  ###############

## install the MIME type spec file for KAlarm specific MIME types
install(FILES kalarm-mime.xml DESTINATION ${XDG_MIME_INSTALL_DIR})
update_xdg_mimetypes(${XDG_MIME_INSTALL_DIR})
