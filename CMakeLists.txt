cmake_minimum_required(VERSION 3.16 FATAL_ERROR)
set(PIM_VERSION "5.240.40")
set(PIM_VERSION ${PIM_VERSION})
set(RELEASE_SERVICE_VERSION_MAJOR "23")
set(RELEASE_SERVICE_VERSION_MINOR "07")
set(RELEASE_SERVICE_VERSION_MICRO "70")
set(RELEASE_SERVICE_VERSION "${RELEASE_SERVICE_VERSION_MAJOR}.${RELEASE_SERVICE_VERSION_MINOR}.${RELEASE_SERVICE_VERSION_MICRO}")

# Whenever KALARM_VERSION changes, set the 3 variables below it to the first
# KDE Release Service version which will contain that KAlarm release.
# (This must be the full release version, not an alpha or beta release.)
set(KALARM_VERSION "3.5.5")
set(KALARM_VERSION_RELEASE_SERVICE_MAJOR "23")
set(KALARM_VERSION_RELEASE_SERVICE_MINOR "04")
set(KALARM_VERSION_RELEASE_SERVICE_MICRO "0")

# If KAlarm's version has not changed since the last KDE Release Service version,
# this code sets a sub-version x.x.x.N to avoid duplicate version numbers in
# org.kde.kalarm.appdata.xml, which cause appstream to choke.
math(EXPR LAST_RELEASE_MONTH "${KALARM_VERSION_RELEASE_SERVICE_MAJOR} * 12 + ${KALARM_VERSION_RELEASE_SERVICE_MINOR} + ${KALARM_VERSION_RELEASE_SERVICE_MICRO}")
math(EXPR THIS_RELEASE_MONTH "${RELEASE_SERVICE_VERSION_MAJOR} * 12 + ${RELEASE_SERVICE_VERSION_MINOR} + ${RELEASE_SERVICE_VERSION_MICRO}")
math(EXPR KALARM_RELEASE_SUB_VERSION "${THIS_RELEASE_MONTH} - ${LAST_RELEASE_MONTH}")
if (${KALARM_RELEASE_SUB_VERSION} GREATER 0)
    set(KALARM_VERSION "${KALARM_VERSION}.${KALARM_RELEASE_SUB_VERSION}")
endif()

project(kalarm VERSION ${KALARM_VERSION})

set(KF_MIN_VERSION "5.240.0")

find_package(ECM ${KF_MIN_VERSION} CONFIG REQUIRED)
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
set(QT_REQUIRED_VERSION "6.4.0")
set(KTEXTADDONS_MIN_VERSION "1.3.41")
# Do NOT add quote
set(KDEPIM_DEV_VERSION alpha)

# add an extra space
if (DEFINED KDEPIM_DEV_VERSION)
    set(KDEPIM_DEV_VERSION " ${KDEPIM_DEV_VERSION}")
endif()


set(KDEPIM_VERSION "${PIM_VERSION}${KDEPIM_DEV_VERSION} (${RELEASE_SERVICE_VERSION})")
set(AKONADI_VERSION "5.240.40")
set(AKONADI_MIMELIB_VERSION "5.240.41")
set(AKONADI_CONTACT_VERSION "5.240.40")
set(KMAILTRANSPORT_LIB_VERSION "5.240.40")
set(KPIMTEXTEDIT_LIB_VERSION "5.240.40")
set(IDENTITYMANAGEMENT_LIB_VERSION "5.240.40")
set(KMIME_LIB_VERSION "5.240.40")

set(KDEPIM_LIB_VERSION "${PIM_VERSION}")
set(KDEPIM_LIB_SOVERSION "5")

find_package(Qt6 ${QT_REQUIRED_VERSION} CONFIG REQUIRED DBus Gui Network Widgets)
set(CALENDARUTILS_LIB_VERSION "5.240.40")

find_package(Qt6Core5Compat)


option(ENABLE_AKONADI_PLUGIN "Build the Akonadi plugin" ON)
add_feature_info(AKONADI_PLUGIN ${ENABLE_AKONADI_PLUGIN} "Provides features dependent on Akonadi (including birthday import,
                   email functions, email address book, first run resource migration)")

option(ENABLE_WAKE_FROM_SUSPEND "Enable wake-from-suspend alarms (requires KAuth)" ON)
if (ENABLE_WAKE_FROM_SUSPEND)
    find_package(KF6Auth ${KF_MIN_VERSION})
    if (NOT KF6Auth_FOUND)
        set(ENABLE_WAKE_FROM_SUSPEND OFF)
    endif()
endif()
add_feature_info(WAKE_FROM_SUSPEND ${ENABLE_WAKE_FROM_SUSPEND} "Wake-from-suspend alarms")

# Find KF6 packages
find_package(KF6CalendarCore ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Codecs ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Completion ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Config ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6ConfigWidgets ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Contacts ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Crash ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6DBusAddons ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6GlobalAccel ${KF_MIN_VERSION} REQUIRED)
find_package(KF6GuiAddons ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Holidays ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6I18n ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6IdleTime ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6ItemModels ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6JobWidgets ${KF_MIN_VERSION} REQUIRED)
find_package(KF6KCMUtils ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6KIO ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Notifications ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6NotifyConfig ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6Service ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6WidgetsAddons ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6WindowSystem ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(KF6XmlGui ${KF_MIN_VERSION} CONFIG REQUIRED)
find_package(Phonon4Qt6 CONFIG REQUIRED)

find_package(KF6DocTools ${KF_MIN_VERSION})
set_package_properties(KF6DocTools PROPERTIES DESCRIPTION
    "Tools to generate documentation"
    TYPE OPTIONAL
)

# Find KDE PIM packages
find_package(KPim6CalendarUtils ${CALENDARUTILS_LIB_VERSION} CONFIG REQUIRED)
find_package(KPim6IdentityManagementWidgets ${IDENTITYMANAGEMENT_LIB_VERSION} CONFIG REQUIRED)
find_package(KPim6Mime ${KMIME_LIB_VERSION} CONFIG REQUIRED)
find_package(KPim6TextEdit ${KPIMTEXTEDIT_LIB_VERSION} CONFIG REQUIRED)

find_package(KF6TextEditTextToSpeech ${KTEXTADDONS_MIN_VERSION} CONFIG)
set_package_properties(KF6TextEditTextToSpeech PROPERTIES DESCRIPTION
    "Add support for text to speech"
    TYPE OPTIONAL
)

if (TARGET KF6::TextEditTextToSpeech)
    add_definitions(-DHAVE_TEXT_TO_SPEECH_SUPPORT)
endif()

if (ENABLE_AKONADI_PLUGIN)
    find_package(KPim6Akonadi ${AKONADI_VERSION} CONFIG REQUIRED)
    find_package(KPim6AkonadiContact ${AKONADI_CONTACT_VERSION} CONFIG REQUIRED)
    find_package(KPim6AkonadiMime ${AKONADI_MIMELIB_VERSION} CONFIG REQUIRED)
    find_package(KPim6MailTransport ${KMAILTRANSPORT_LIB_VERSION} CONFIG REQUIRED)
endif()

configure_file(kalarm-version-string.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kalarm-version-string.h @ONLY)

if (NOT APPLE)
    option(WITHOUT_X11 "Build without X11 integration (skips finding X11)" OFF)
    add_feature_info(WITHOUT_X11 ${WITHOUT_X11} "Disable X11 integration, even if X11 is available")
    if (NOT WITHOUT_X11)
        find_package(X11)
        set(ENABLE_X11 ${X11_FOUND})
        if (X11_FOUND)
            find_package(Qt6 ${QT_REQUIRED_VERSION} CONFIG REQUIRED Gui)
            add_feature_info(ENABLE_X11 ${ENABLE_X11} "Enable use of X11")
        endif()
    endif()
endif()

set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

configure_file(src/config-kalarm.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kalarm.h)

include_directories(${kalarm_SOURCE_DIR} ${kalarm_BINARY_DIR})

add_definitions(-DQT_MESSAGELOGCONTEXT)
ecm_set_disabled_deprecation_versions(QT 5.15.2  KF 5.240.0)

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

if (KF6DocTools_FOUND)
    kdoctools_install(po)
    add_subdirectory(doc)
endif()
ki18n_install(po)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
