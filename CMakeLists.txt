project(kalarm)

add_definitions(-DQT_STRICT_ITERATORS)
add_definitions(-DQT_NO_CAST_FROM_ASCII)
add_definitions(-DQT_NO_CAST_TO_ASCII)
add_definitions(-DQT_MESSAGELOGCONTEXT)

set(CMAKE_AUTOMOC_RELAXED_MODE ON)

add_subdirectory(src)

if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()
