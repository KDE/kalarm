include(ECMMarkAsTest)

find_package(Qt5DBus CONFIG REQUIRED)
find_package(Qt5Test CONFIG REQUIRED)

macro(macro_unit_tests)
  foreach(_testname ${ARGN})
    add_executable(${_testname} ${_testname}.cpp)
    add_test(NAME ${_testname} COMMAND ${_testname})
    ecm_mark_as_test(${_testname})
    target_link_libraries(${_testname}
        KF5AlarmCalendar
        Qt5::DBus
        Qt5::Test)
  endforeach()
endmacro()

macro_unit_tests(
    kadatetimetest
    kaeventtest
)
