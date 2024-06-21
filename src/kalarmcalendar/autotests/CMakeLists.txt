# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: none
include(ECMMarkAsTest)

find_package(Qt6DBus CONFIG REQUIRED)
find_package(Qt6Test CONFIG REQUIRED)

macro(macro_unit_tests)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp ${_testname}.h)
    add_test(NAME ${_testname} COMMAND ${_testname})
    ecm_mark_as_test(${_testname})
    target_link_libraries(${_testname}
        KF6::CalendarCore
        KF6::Holidays
        kalarmcalendar
        Qt::DBus
        Qt::Test)
    target_include_directories(${_testname} PUBLIC "$<BUILD_INTERFACE:${kalarm_SOURCE_DIR}/src/kalarmcalendar;${kalarm_BINARY_DIR}/src/kalarmcalendar>")
  endforeach()
endmacro()
if(NOT WIN32)
macro_unit_tests(
    kadatetimetest
    kaeventtest
)
else()
    message(STATUS "REACTIVATE AUTOTEST on WINDOWS")
endif()
