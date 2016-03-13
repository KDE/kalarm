cmake_minimum_required(VERSION 2.8.12)

project(KAlarmCal)

# ECM setup
find_package(ECM 5.19.0 CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMGeneratePriFile)
include(ECMPackageConfigHelpers)
include(ECMSetupVersion)
include(FeatureSummary)
include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(ECMQtDeclareLoggingCategory)

set(KF5_VERSION "5.19.0")
set(KALARM_LIB_VERSION "5.1.80")
set(CALENDARCORE_LIB_VERSION "5.1.80")
set(HOLIDAY_LIB_VERSION "5.1.80")
set(IDENTITYMANAGER_LIB_VERSION "5.1.80")
set(AKONADI_LIB_VERSION "5.1.80")
ecm_setup_version(${KALARM_LIB_VERSION} VARIABLE_PREFIX KALARMCAL
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5AlarmCalendarConfigVersion.cmake"
                        SOVERSION 5
)

########### Find packages ###########
find_package(KF5KDELibs4Support ${KF5_VERSION} CONFIG REQUIRED)

find_package(KF5Akonadi ${AKONADI_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5CalendarCore ${CALENDARCORE_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5IdentityManagement ${IDENTITYMANAGER_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5Holidays ${HOLIDAY_LIB_VERSION} CONFIG REQUIRED)

add_definitions(-DTRANSLATION_DOMAIN=\"libkalarmcal5\")
add_definitions("-DQT_NO_CAST_FROM_ASCII -DQT_NO_CAST_TO_ASCII")
########### Targets ###########

add_subdirectory(src)

########### CMake Config Files ###########
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF5AlarmCalendar")

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
  DESTINATION ${KDE_INSTALL_INCLUDEDIR_KF5} COMPONENT Devel
)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

