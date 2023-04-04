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
    INSTALL_NAMESPACE "pim${QT_MAJOR_VERSION}/kalarm"
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
    KF6::CalendarCore
    KF6::I18n
    KF6::Codecs
    KPim6::Mime
    KPim6::AkonadiCore
    KPim6::AkonadiMime
    KPim6::AkonadiContact
    KPim6::AkonadiWidgets
    KPim6::IdentityManagement
    KPim6::MailTransport
)
