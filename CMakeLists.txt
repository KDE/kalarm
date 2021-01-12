cmake_minimum_required(VERSION 3.5 FATAL_ERROR)
set(PIM_VERSION "5.16.40")

project(KAlarmCal VERSION ${PIM_VERSION})

# ECM setup
set(KF5_MIN_VERSION "5.78.0")

find_package(ECM ${KF5_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)

include(GenerateExportHeader)
include(ECMGenerateHeaders)
include(ECMGeneratePriFile)

include(ECMSetupVersion)
include(FeatureSummary)
include(ECMQtDeclareLoggingCategory)


set(CALENDARUTILS_LIB_VERSION "5.16.40")
set(IDENTITYMANAGER_LIB_VERSION "5.16.40")
set(AKONADI_LIB_VERSION "5.16.40")
ecm_setup_version(PROJECT VARIABLE_PREFIX KALARMCAL
                        VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/kalarmcal_version.h"
                        PACKAGE_VERSION_FILE "${CMAKE_CURRENT_BINARY_DIR}/KF5AlarmCalendarConfigVersion.cmake"
                        SOVERSION 5
)

########### Find packages ###########
find_package(KF5CalendarCore ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Config ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Holidays ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5I18n ${KF5_MIN_VERSION} CONFIG REQUIRED)

find_package(KF5Akonadi ${AKONADI_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5IdentityManagement ${IDENTITYMANAGER_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5CalendarUtils ${CALENDARUTILS_LIB_VERSION} CONFIG REQUIRED)

add_definitions(-DQT_NO_FOREACH)
add_definitions(-DQT_NO_KEYWORDS)
add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050f00)
add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x054F00)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
option(USE_UNITY_CMAKE_SUPPORT "Use UNITY cmake support (speedup compile time)" FALSE)

set(COMPILE_WITH_UNITY_CMAKE_SUPPORT false)
if (USE_UNITY_CMAKE_SUPPORT)
    if(${CMAKE_VERSION} VERSION_LESS "3.16.0")
        message(STATUS "CMAKE version is less than 3.16.0 . We can't use cmake unify build support")
    else()
        set(COMPILE_WITH_UNITY_CMAKE_SUPPORT true)
    endif()
endif()
########### Targets ###########

add_subdirectory(src)
add_subdirectory(serializers)

if(BUILD_TESTING)
   add_subdirectory(autotests)
endif()

########### CMake Config Files ###########
set(CMAKECONFIG_INSTALL_DIR "${KDE_INSTALL_CMAKEPACKAGEDIR}/KF5AlarmCalendar")

configure_package_config_file(
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

ecm_qt_install_logging_categories(
        EXPORT KALARMCAL
        FILE kalarmcal.categories
        DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
        )


feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
