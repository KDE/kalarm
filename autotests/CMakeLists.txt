include(ECMMarkAsTest)

find_package(Qt5DBus CONFIG REQUIRED)
find_package(Qt5Test CONFIG REQUIRED)

########### next target ###############

set(kadatetimetest_SRCS kadatetimetest.cpp)

add_executable(kadatetimetest ${kadatetimetest_SRCS})
add_test(kadatetimetest kadatetimetest)
ecm_mark_as_test(kadatetimetest)
target_link_libraries(kadatetimetest KF5::AlarmCalendar Qt5::DBus Qt5::Test)
