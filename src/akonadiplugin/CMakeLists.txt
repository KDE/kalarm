# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: none

# This builds KAlarm's Akonadi plugin, which provides all functions dependent on
# Akonadi.

kcoreaddons_add_plugin(akonadiplugin
    SOURCES
        akonadiplugin.cpp
        akonadicollectionsearch.cpp
        akonadiresourcemigrator.cpp
        birthdaymodel.cpp
        collectionattribute.cpp
        sendakonadimail.cpp
        akonadiplugin.h
        akonadicollectionsearch.h
        akonadiresourcemigrator.h
        birthdaymodel.h
        collectionattribute.h
        sendakonadimail.h
    INSTALL_NAMESPACE "kalarm"
)

set_target_properties(akonadiplugin PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins/kalarm")

ecm_qt_declare_logging_category(akonadiplugin
                                HEADER akonadiplugin_debug.h
                                IDENTIFIER AKONADIPLUGIN_LOG
                                CATEGORY_NAME org.kde.pim.kalarm.akonadiplugin
                                DEFAULT_SEVERITY Warning
                                DESCRIPTION "kalarm (akonadi plugin)"
                                EXPORT AKONADIPLUGIN
                               )

generate_export_header(akonadiplugin BASE_NAME akonadiplugin)

target_link_libraries(akonadiplugin
    kalarmplugin
    kalarmcalendar
    KF5::CalendarCore
    KF5::I18n
    KF5::Mime
    KF5::AkonadiCore
    KF5::AkonadiMime
    KF5::AkonadiContact
    KF5::AkonadiWidgets
    KF5::IdentityManagement
    KF5::MailTransportAkonadi
    KF5::MailTransport
)