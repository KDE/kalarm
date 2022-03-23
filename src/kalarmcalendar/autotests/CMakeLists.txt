# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: none
include(ECMMarkAsTest)

find_package(Qt${QT_MAJOR_VERSION}DBus CONFIG REQUIRED)
find_package(Qt${QT_MAJOR_VERSION}Test CONFIG REQUIRED)

macro(macro_unit_tests)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp ${_testname}.h)
    add_test(NAME ${_testname} COMMAND ${_testname})
    ecm_mark_as_test(${_testname})
    target_link_libraries(${_testname}
        KF5::CalendarCore
        KF5::Holidays
        kalarmprivate
        Qt${QT_MAJOR_VERSION}::DBus
        Qt${QT_MAJOR_VERSION}::Test)
  endforeach()
endmacro()
if (NOT WIN32)
macro_unit_tests(
    kadatetimetest
    kaeventtest
)
else()
    MESSAGE(STATUS "REACTIVATE AUTOTEST on WINDOWS")
endif()
