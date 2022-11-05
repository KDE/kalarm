cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(PIM_VERSION "5.21.40")
set(PIM_VERSION ${PIM_VERSION})
set(RELEASE_SERVICE_VERSION_MAJOR "22")
set(RELEASE_SERVICE_VERSION_MINOR "11")
set(RELEASE_SERVICE_VERSION_MICRO "80")
set(RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

set(KALARM_VERSION "3.5.3")
# Whenever KALARM_VERSION changes, set KALARM_RELEASE_SUB_VERSION to "".
# If KALARM_VERSION has not changed since the last KDE release service release,
# increment KALARM_RELEASE_SUB_VERSION (to "1", "2", ...) to ensure that KAlarm
# always has a different version for every KDE release.
set(KALARM_RELEASE_SUB_VERSION "")
if (NOT ${KALARM_RELEASE_SUB_VERSION} EQUAL "")
    set(KALARM_VERSION "${KALARM_VERSION}.${KALARM_RELEASE_SUB_VERSION}")
endif()

project(kalarm VERSION ${KALARM_VERSION})

set(KF5_MIN_VERSION "5.99.0")

find_package(ECM ${KF5_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${kalarm_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})
include(ECMInstallIcons)

include(ECMSetupVersion)
include(ECMAddTests)

include(GenerateExportHeader)
include(ECMGenerateHeaders)

include(FeatureSummary)
include(CheckFunctionExists)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(ECMAddAppIcon)
include(ECMQtDeclareLoggingCategory)
include(ECMDeprecationSettings)

# Do NOT add quote
set(KDEPIM_DEV_VERSION alpha)

# add an extra space
if (DEFINED KDEPIM_DEV_VERSION)
    set(KDEPIM_DEV_VERSION " ${KDEPIM_DEV_VERSION}")
endif()


set(KDEPIM_VERSION "${PIM_VERSION}${KDEPIM_DEV_VERSION} (${RELEASE_SERVICE_VERSION})")
set(AKONADI_VERSION "5.21.40")
set(AKONADI_MIMELIB_VERSION "5.21.40")
set(AKONADI_CONTACT_VERSION "5.21.40")
set(KMAILTRANSPORT_LIB_VERSION "5.21.40")
set(KPIMTEXTEDIT_LIB_VERSION "5.21.40")
set(IDENTITYMANAGEMENT_LIB_VERSION "5.21.40")
set(KMIME_LIB_VERSION "5.21.40")

set(KDEPIM_LIB_VERSION "${PIM_VERSION}")
set(KDEPIM_LIB_SOVERSION "5")

set(QT_REQUIRED_VERSION "5.15.2")
if (QT_MAJOR_VERSION STREQUAL "6")
    set(QT_REQUIRED_VERSION "6.4.0")
endif()
find_package(Qt${QT_MAJOR_VERSION} ${QT_REQUIRED_VERSION} CONFIG REQUIRED DBus Gui Network Widgets)
set(CALENDARUTILS_LIB_VERSION "5.21.40")

option(ENABLE_AKONADI_PLUGIN "Build the Akonadi plugin" ON)
add_feature_info(AKONADI_PLUGIN ${ENABLE_AKONADI_PLUGIN} "Provides features dependent on Akonadi (including birthday import,
                   email functions, email address book, first run resource migration)")

option(ENABLE_WAKE_FROM_SUSPEND "Enable wake-from-suspend alarms (requires KAuth)" ON)
if (ENABLE_WAKE_FROM_SUSPEND)
    find_package(KF5Auth ${KF5_MIN_VERSION})
    if (NOT KF5Auth_FOUND)
        set(ENABLE_WAKE_FROM_SUSPEND OFF)
    endif()
endif()
add_feature_info(WAKE_FROM_SUSPEND ${ENABLE_WAKE_FROM_SUSPEND} "Wake-from-suspend alarms")

# Find KF5 packages
find_package(KF5CalendarCore ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Codecs ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Completion ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5Config ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5ConfigWidgets ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Contacts ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Crash ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5DBusAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5GlobalAccel ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5GuiAddons ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5Holidays ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5I18n ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5IdleTime ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5ItemModels ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5JobWidgets ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5KCMUtils ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5KIO ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Notifications ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5NotifyConfig ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Service ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5WidgetsAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5WindowSystem ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5XmlGui ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(Phonon4Qt${QT_MAJOR_VERSION} CONFIG REQUIRED)

find_package(KF5DocTools ${KF5_MIN_VERSION})
set_package_properties(KF5DocTools PROPERTIES DESCRIPTION
    "Tools to generate documentation"
    TYPE OPTIONAL
)

# Find KDE PIM packages
find_package(KF5CalendarUtils ${CALENDARUTILS_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5IdentityManagement ${IDENTITYMANAGEMENT_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5Mime ${KMIME_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5PimTextEdit ${KPIMTEXTEDIT_LIB_VERSION} CONFIG REQUIRED)
if (ENABLE_AKONADI_PLUGIN)
    find_package(KF5Akonadi ${AKONADI_VERSION} CONFIG REQUIRED)
    find_package(KF5AkonadiContact ${AKONADI_CONTACT_VERSION} CONFIG REQUIRED)
    find_package(KF5AkonadiMime ${AKONADI_MIMELIB_VERSION} CONFIG REQUIRED)
    find_package(KF5MailTransportAkonadi ${KMAILTRANSPORT_LIB_VERSION} CONFIG REQUIRED)
endif()

configure_file(kalarm-version-string.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kalarm-version-string.h @ONLY)

if (NOT APPLE)
    option(WITHOUT_X11 "Build without X11 integration (skips finding X11)" OFF)
    add_feature_info(WITHOUT_X11 ${WITHOUT_X11} "Disable X11 integration, even if X11 is available")
    if (NOT WITHOUT_X11)
        find_package(X11)
        set(ENABLE_X11 ${X11_FOUND})
        if (X11_FOUND)
            if (QT_MAJOR_VERSION STREQUAL "5")
                find_package(Qt5X11Extras ${QT_REQUIRED_VERSION} REQUIRED NO_MODULE)
            else()
                find_package(Qt${QT_MAJOR_VERSION} ${QT_REQUIRED_VERSION} CONFIG REQUIRED Gui)
            endif()
            add_feature_info(ENABLE_X11 ${ENABLE_X11} "Enable use of X11")
        endif()
    endif()
endif()

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

configure_file(src/config-kalarm.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kalarm.h)

include_directories(${kalarm_SOURCE_DIR} ${kalarm_BINARY_DIR})

add_definitions(-DQT_MESSAGELOGCONTEXT)
ecm_set_disabled_deprecation_versions(QT 5.15.2 KF 5.99.0)

option(USE_UNITY_CMAKE_SUPPORT "Use UNITY cmake support (speedup compile time)" OFF)

set(COMPILE_WITH_UNITY_CMAKE_SUPPORT OFF)
if (USE_UNITY_CMAKE_SUPPORT)
    set(COMPILE_WITH_UNITY_CMAKE_SUPPORT ON)
endif()

add_subdirectory(src)

ecm_qt_install_logging_categories(
    EXPORT KALARM
    FILE kalarm.categories
    DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
    )

if (KF5DocTools_FOUND)
    kdoctools_install(po)
    add_subdirectory(doc)
endif()
ki18n_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
