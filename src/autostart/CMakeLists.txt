include_directories( ../  )

########### next target ###############

set(kalarmautostart_SRCS
    autostart.cpp
)

if (ECM_VERSION VERSION_LESS "5.68.0")
    ecm_qt_declare_logging_category(kalarmautostart_SRCS HEADER kalarm_autostart_debug.h IDENTIFIER KALARMAUTOSTART_LOG CATEGORY_NAME org.kde.pim.autostart_kalarm)
else()
    ecm_qt_declare_logging_category(kalarmautostart_SRCS HEADER kalarm_autostart_debug.h IDENTIFIER KALARMAUTOSTART_LOG CATEGORY_NAME org.kde.pim.autostart_kalarm
                                    DESCRIPTION "kalarm (autostart_kalarm)"
                                    OLD_CATEGORY_NAMES log_autostart_kalarm
                                    EXPORT KALARM
                                    )
endif()


file(GLOB ICONS_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/../appicons/*-apps-kalarm.png")
ecm_add_app_icon(kalarmautostart_SRCS ICONS ${ICONS_SRCS})

add_executable(kalarmautostart ${kalarmautostart_SRCS})

target_link_libraries(kalarmautostart
    KF5::I18n
    KF5::CoreAddons
    Qt5::Widgets
    Qt5::DBus)

install(TARGETS kalarmautostart  ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} )
