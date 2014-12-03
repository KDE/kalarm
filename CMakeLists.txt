cmake_minimum_required(VERSION 2.8.12)

project(KAlarmCal)

# ECM setup
find_package(ECM 1.3.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMGeneratePriFile)
include(ECMPackageConfigHelpers)
include(ECMSetupVersion)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings)

set(KF5_VERSION "5.5.0")
set(KDEPIMLIBS_VERSION "4.77.0")

ecm_setup_version(${KDEPIMLIBS_VERSION} VARIABLE_PREFIX KALARMCAL
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5AlarmCalendarConfigVersion.cmake"
                        SOVERSION 5
)

########### Find packages ###########
find_package(KF5KDELibs4Support ${KF5_VERSION} CONFIG REQUIRED)

if(${CMAKE_SOURCE_DIR} STREQUAL ${KAlarmCal_SOURCE_DIR})
  find_package(KF5Akonadi ${KDEPIMLIBS_VERSION} CONFIG REQUIRED)
  find_package(KF5CalendarCore ${KDEPIMLIBS_VERSION} CONFIG REQUIRED)
  find_package(KF5IdentityManagement ${KDEPIMLIBS_VERSION} CONFIG REQUIRED)
  find_package(KF5Holidays ${KDEPIMLIBS_VERSION} CONFIG REQUIRED)
else()
  # IMPORTED targets are scoped, so KF5::AkonadiPrivate found in ../akonadi
  # won't work here, and it is a PUBLIC dependency of KF5::Akonadi
  find_package(KF5AkonadiServer ${KF5AkonadiServer_MIN_VERSION} CONFIG REQUIRED)
endif()

add_definitions(-DTRANSLATION_DOMAIN=\"libkalarmcal5\")
add_definitions("-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII")
########### Targets ###########

add_subdirectory(src)

########### CMake Config Files ###########
set(CMAKECONFIG_INSTALL_DIR "${CMAKECONFIG_INSTALL_PREFIX}/KF5AlarmCalendar")

ecm_configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/KF5AlarmCalendarConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5AlarmCalendarConfig.cmake"
  INSTALL_DESTINATION  ${CMAKECONFIG_INSTALL_DIR}
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/KF5AlarmCalendarConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/KF5AlarmCalendarConfigVersion.cmake"
  DESTINATION "${CMAKECONFIG_INSTALL_DIR}"
  COMPONENT Devel
)

install(EXPORT KF5AlarmCalendarTargets DESTINATION "${CMAKECONFIG_INSTALL_DIR}" FILE KF5AlarmCalendarTargets.cmake NAMESPACE KF5::)

install(FILES
   ${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_version.h
  DESTINATION ${KF5_INCLUDE_INSTALL_DIR} COMPONENT Devel
)

if ("${CMAKE_BINARY_DIR}" STREQUAL "${CMAKE_CURRENT_BINARY_DIR}")
  feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
endif()

