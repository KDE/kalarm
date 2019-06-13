cmake_minimum_required(VERSION 3.5)
set(KDEPIM_VERSION_NUMBER "5.11.3")
set(PIM_VERSION ${KDEPIM_VERSION_NUMBER})

project(kalarm VERSION "2.12.4")

set(KF5_MIN_VERSION "5.56.0")

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
set(KDEPIM_DEV_VERSION )

# add an extra space
if(DEFINED KDEPIM_DEV_VERSION)
    set(KDEPIM_DEV_VERSION " ${KDEPIM_DEV_VERSION}")
endif()


set(KDEPIM_VERSION "${KDEPIM_VERSION_NUMBER}${KDEPIM_DEV_VERSION}")
set(KIMAP_LIB_VERSION "5.11.2")
set(AKONADI_MIMELIB_VERSION "5.11.2")
set(AKONADI_CONTACT_VERSION "5.11.2")

set(KMAILTRANSPORT_LIB_VERSION "5.11.2")
set(KPIMTEXTEDIT_LIB_VERSION "5.11.2")
set(IDENTITYMANAGEMENT_LIB_VERSION "5.11.2")
set(AKONADI_VERSION "5.11.2")
set(KMIME_LIB_VERSION "5.11.2")
set(AKONADIKALARM_LIB_VERSION "5.11.2")
set(PIMCOMMON_LIB_VERSION_LIB "5.11.2")

set(KDEPIM_LIB_VERSION "${KDEPIM_VERSION_NUMBER}")
set(KDEPIM_LIB_SOVERSION "5")

set(QT_REQUIRED_VERSION "5.10.0")
find_package(Qt5 ${QT_REQUIRED_VERSION} CONFIG REQUIRED DBus Gui Network Widgets)
find_package(Qt5X11Extras NO_MODULE)
set(MAILCOMMON_LIB_VERSION_LIB "5.11.2")
set(LIBKDEPIM_LIB_VERSION_LIB "5.11.2")
set(KCALENDARCORE_LIB_VERSION "5.11.2")
set(CALENDARUTILS_LIB_VERSION "5.11.2")
set(KDEPIM_APPS_LIB_VERSION_LIB "5.11.2")

# Find KF5 package
find_package(KF5Auth ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Codecs ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5Completion ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5Config ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5ConfigWidgets ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5DBusAddons ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5DocTools ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5GlobalAccel ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5GuiAddons ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5I18n ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5IconThemes ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5JobWidgets ${KF5_MIN_VERSION} REQUIRED)
find_package(KF5KCMUtils ${KF5_MIN_VERSION} CONFIG REQUIRED)
find_package(KF5KDELibs4Support ${KF5_MIN_VERSION} CONFIG REQUIRED)
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
find_package(KF5KdepimDBusInterfaces ${KDEPIM_APPS_LIB_VERSION_LIB} CONFIG REQUIRED)
find_package(KF5Libkdepim ${LIBKDEPIM_LIB_VERSION_LIB} CONFIG REQUIRED)
find_package(KF5MailCommon ${MAILCOMMON_LIB_VERSION_LIB} CONFIG REQUIRED)
find_package(KF5MailTransportAkonadi ${KMAILTRANSPORT_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5Mime ${KMIME_LIB_VERSION} CONFIG REQUIRED)
find_package(KF5PimCommon ${PIMCOMMON_LIB_VERSION_LIB} CONFIG REQUIRED)
find_package(KF5PimTextEdit ${KPIMTEXTEDIT_LIB_VERSION} CONFIG REQUIRED)


if (NOT APPLE)
    find_package(X11)
endif()
set(CMAKE_MODULE_PATH ${kalarm_SOURCE_DIR}/cmake/modules ${ECM_MODULE_PATH})
find_package(Xsltproc)
set_package_properties(Xsltproc PROPERTIES DESCRIPTION "XSLT processor from libxslt" TYPE REQUIRED PURPOSE "Required to generate D-Bus interfaces for all Akonadi resources.")
set(KDEPIM_HAVE_X11 ${X11_FOUND})
configure_file(src/config-kalarm.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config-kalarm.h )

include_directories(${kalarm_SOURCE_DIR} ${kalarm_BINARY_DIR})

add_definitions(-DQT_MESSAGELOGCONTEXT)


add_subdirectory(src)

install( FILES kalarm.renamecategories kalarm.categories DESTINATION ${KDE_INSTALL_CONFDIR} )

add_subdirectory(doc)
feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)
