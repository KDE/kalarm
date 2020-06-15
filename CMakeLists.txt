cmake_minimum_required(VERSION 3.5)
set(PIM_VERSION "5.14.40")
set(PIM_VERSION ${PIM_VERSION})
set(RELEASE_SERVICE_VERSION "20.07.40")
set(KALARM_VERSION "3.0.0")

project(kalarm VERSION ${KALARM_VERSION})

set(KF5_MIN_VERSION "5.71.0")

find_package(ECM ${KF5_MIN_VERSION} CONFIG REQUIRED)
set(CMAKE_MODULE_PATH ${kalarm_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})
include(ECMInstallIcons)


include(ECMSetupVersion)
include(ECMAddTests)

include(GenerateExportHeader)
include(ECMGenerateHeaders)

include(FeatureSummary)
include(CheckFunctionExists)
include(ECMGeneratePriFile)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDEFrameworkCompilerSettings NO_POLICY_SCOPE)
include(ECMAddAppIcon)
include(ECMQtDeclareLoggingCategory)

# Do NOT add quote
set(KDEPIM_DEV_VERSION alpha)

# add an extra space
if(DEFINED KDEPIM_DEV_VERSION)
    set(KDEPIM_DEV_VERSION " ${KDEPIM_DEV_VERSION}")
endif()


set(KDEPIM_VERSION "${PIM_VERSION}${KDEPIM_DEV_VERSION} (${RELEASE_SERVICE_VERSION})")
set(KIMAP_LIB_VERSION "5.14.40")
set(AKONADI_MIMELIB_VERSION "5.14.40")
set(AKONADI_CONTACT_VERSION "5.14.40")

set(KMAILTRANSPORT_LIB_VERSION "5.14.40")
set(KPIMTEXTEDIT_LIB_VERSION "5.14.40")
set(IDENTITYMANAGEMENT_LIB_VERSION "5.14.40")
set(AKONADI_VERSION "5.14.40")
set(KMIME_LIB_VERSION "5.14.40")
set(AKONADIKALARM_LIB_VERSION "5.14.40")
set(PIMCOMMON_LIB_VERSION_LIB "5.14.40")

set(KDEPIM_LIB_VERSION "${PIM_VERSION}")
set(KDEPIM_LIB_SOVERSION "5")

set(QT_REQUIRED_VERSION "5.13.0")
find_package(Qt5 ${QT_REQUIRED_VERSION} CONFIG REQUIRED DBus Gui Network Widgets)
find_package(Qt5X11Extras NO_MODULE)
set(MAILCOMMON_LIB_VERSION_LIB "5.14.40")
set(KCALENDARCORE_LIB_VERSION "5.14.40")
set(CALENDARUTILS_LIB_VERSION "5.14.40")

option(FILE_RESOURCES "Use file system resources instead of Akonadi resources." TRUE)

# Find KF5 package
find_package(KF5Auth ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Codecs ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Completion ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5Config ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5ConfigWidgets ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Crash ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5DBusAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5DocTools ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5GlobalAccel ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5GuiAddons ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5JobWidgets ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5KCMUtils ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5KIO ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Notifications ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Service ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5WidgetsAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5WindowSystem ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5XmlGui ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Holidays ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(Phonon4Qt5 CONFIG REQUIRED)

# Find KdepimLibs Package
find_package(KF5IMAP ${KIMAP_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5Akonadi ${AKONADI_VERSION} CONFIG REQUIRED)
find_package(KF5AkonadiContact ${AKONADI_CONTACT_VERSION} CONFIG REQUIRED)
find_package(KF5AkonadiMime ${AKONADI_MIMELIB_VERSION} CONFIG REQUIRED)
find_package(KF5AlarmCalendar ${AKONADIKALARM_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5CalendarCore ${KCALENDARCORE_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5CalendarUtils ${CALENDARUTILS_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5IdentityManagement ${IDENTITYMANAGEMENT_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5MailCommon ${MAILCOMMON_LIB_VERSION_LIB} CONFIG REQUIRED)
find_package(KF5MailTransportAkonadi ${KMAILTRANSPORT_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5Mime ${KMIME_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5PimCommon ${PIMCOMMON_LIB_VERSION_LIB} CONFIG REQUIRED)
find_package(KF5PimTextEdit ${KPIMTEXTEDIT_LIB_VERSION} CONFIG REQUIRED)
configure_file(kalarm-version-string.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/kalarm-version-string.h @ONLY)

if (NOT APPLE)
    find_package(X11)
endif()
set(CMAKE_MODULE_PATH ${kalarm_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})
find_package(Xsltproc)
set_package_properties(Xsltproc PROPERTIES DESCRIPTION "XSLT processor from libxslt" TYPE REQUIRED PURPOSE "Required to generate D-Bus interfaces for all Akonadi resources.")

set(KDEPIM_HAVE_X11 ${X11_FOUND})
set(FILE_RESOURCES ${FILE_RESOURCES})
configure_file(src/config-kalarm.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kalarm.h)

include_directories(${kalarm_SOURCE_DIR} ${kalarm_BINARY_DIR})

add_definitions(-DQT_MESSAGELOGCONTEXT)
add_definitions(-DQT_NO_FOREACH)
add_definitions(-DQT_NO_KEYWORDS)
if (EXISTS "${CMAKE_SOURCE_DIR}/.git")
	#add_definitions(-DQT_DISABLE_DEPRECATED_BEFORE=0x050f00) # for the moment we can't port QWheelEvent
   add_definitions(-DKF_DISABLE_DEPRECATED_BEFORE_AND_AT=0x054700)
endif()


add_subdirectory(src)

ecm_qt_install_logging_categories(
        EXPORT KALARM
        FILE kalarm.categories
        DESTINATION ${KDE_INSTALL_LOGGINGCATEGORIESDIR}
        )

add_subdirectory(doc)
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
