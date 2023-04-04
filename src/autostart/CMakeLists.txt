include_directories( ../  )

########### next target ###############

add_executable(kalarmautostart ${kalarmautostart_SRCS})
target_sources(kalarmautostart PRIVATE
    autostart.cpp
    autostart.h
)

ecm_qt_declare_logging_category(kalarmautostart
                                HEADER kalarm_autostart_debug.h
                                IDENTIFIER KALARMAUTOSTART_LOG
                                CATEGORY_NAME org.kde.pim.autostart_kalarm
                                DESCRIPTION "kalarm (autostart_kalarm)"
                                OLD_CATEGORY_NAMES log_autostart_kalarm
                                EXPORT KALARM
                                )


file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../appicons/*-apps-kalarm.png")
ecm_add_app_icon(kalarmautostart ICONS ${ICONS_SRCS})


target_link_libraries(kalarmautostart
    KF6::I18n
    KF6::CoreAddons
    Qt::Widgets
    Qt::DBus)

install(TARGETS kalarmautostart  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
