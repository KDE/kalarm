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
    holidays.cpp
    identities.cpp
    kacalendar.cpp
    kaevent.cpp
    kadatetime.cpp
    karecurrence.cpp
    repetition.cpp
    version.cpp

    alarmtext.h
    datetime.h
    holidays.h
    identities.h
    kacalendar.h
    kaevent.h
    kadatetime.h
    karecurrence.h
    repetition.h
    version.h
    )


if(COMPILE_WITH_UNITY_CMAKE_SUPPORT)
    set_target_properties(kalarmcalendar PROPERTIES UNITY_BUILD ON)
endif()
generate_export_header(kalarmcalendar BASE_NAME kalarmcal)

if(BUILD_TESTING)
    add_subdirectory(autotests)
endif()

target_link_libraries(kalarmcalendar PRIVATE
    KF6::ConfigCore
    KF6::I18n
    KF6::Holidays
    KPim6::IdentityManagementCore
    KF6::CoreAddons
    KF6::CalendarCore
    )

target_include_directories(kalarmcalendar PUBLIC "$<BUILD_INTERFACE:${kalarm_SOURCE_DIR}/src/kalarmcalendar;${kalarm_BINARY_DIR}/src/kalarmcalendar>")

set_target_properties(kalarmcalendar
    PROPERTIES VERSION ${KDEPIM_LIB_VERSION}
    SOVERSION ${KDEPIM_LIB_SOVERSION}
    )

install(TARGETS kalarmcalendar ${KDE_INSTALL_TARGETS_DEFAULT_ARGS} LIBRARY NAMELINK_SKIP)
