# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: none

# This builds the library kalarmcalendar, which provides access to KAlarm
# calendar data. It used to be the public library kalarmcal.

add_library(kalarmcalendar)
ecm_qt_declare_logging_category(kalarmcalendar
                                HEADER kalarmcal_debug.h
                                IDENTIFIER KALARMCAL_LOG
                                CATEGORY_NAME org.kde.pim.kalarm.kalarmcal
                                DESCRIPTION "kalarm (kalarmcal)"
                                OLD_CATEGORY_NAMES log_kalarmcal
                                EXPORT KALARMCAL
                                )

target_sources(kalarmcalendar PRIVATE
    alarmtext.cpp
    datetime.cpp
    identities.cpp
    kacalendar.cpp
    kaevent.cpp
    kadatetime.cpp
    karecurrence.cpp
    repetition.cpp
    version.cpp

    alarmtext.h
    datetime.h
    identities.h
    kacalendar.h
    kaevent.h
    kadatetime.h
    karecurrence.h
    repetition.h
    version.h
    )


if (COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(kalarmcalendar PROPERTIES UNITY_BUILD ON)
endif()
generate_export_header(kalarmcalendar BASE_NAME kalarmcal)

if (BUILD_TESTING)
    add_subdirectory(autotests)
endif()

target_link_libraries(kalarmcalendar PRIVATE
    KF5::ConfigCore
    KF5::I18n
    KF5::Holidays
    KF5::IdentityManagement
    KF5::CoreAddons
    KF5::CalendarCore
    )

if (QT_MAJOR_VERSION STREQUAL "6")
    target_link_libraries(kalarmcalendar PRIVATE Qt6::Core5Compat)
endif()

target_include_directories(kalarmcalendar PUBLIC "$<BUILD_INTERFACE:${kalarm_SOURCE_DIR}/src/kalarmcalendar;${kalarm_BINARY_DIR}/src/kalarmcalendar>")

set_target_properties(kalarmcalendar
    PROPERTIES VERSION ${KDEPIM_LIB_VERSION}
    SOVERSION ${KDEPIM_LIB_SOVERSION}
    )

install(TARGETS kalarmcalendar ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
