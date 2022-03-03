/*
 *  datamodel.h  -  model independent access to calendar functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kacalendar.h"

class Resource;
class ResourceListModel;
class ResourceFilterCheckListModel;
class AlarmListModel;
class TemplateListModel;
class ResourceCreator;


/*=============================================================================
= Class: DataModel
= Static methods providing model independent access to the resource data model.
=============================================================================*/
class DataModel
{
public:
    /** Initialise the data model. */
    static void initialise();

    /** Terminate access to the data model, and tidy up. */
    static void terminate();

    /** Reload all resources' data from storage.
     *  @note In the case of Akonadi, this does not reload from the backend storage.
     */
    static void reload();

    /** Reload a resource's data from storage.
     *  @note In the case of Akonadi, this does not reload from the backend storage.
     */
    static bool reload(Resource&);

    /** Return whether calendar migration/creation at initialisation has completed. */
    static bool isMigrationComplete();

    /** Check for, and remove, any duplicate Akonadi resources, i.e. those which
     *  use the same calendar file/directory.
     */
    static void removeDuplicateResources();

    /** Disable the widget if the database engine is not available, and display an
     *  error overlay.
     */
    static void widgetNeedsDatabase(QWidget*);

    /** Create a ResourceCreator instance for the model. */
    static ResourceCreator* createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent);

    /** Update a resource's backend calendar file to the current KAlarm format. */
    static void updateCalendarToCurrentFormat(Resource&, bool ignoreKeepFormat, QObject* parent);

    static ResourceListModel* createResourceListModel(QObject* parent);
    static ResourceFilterCheckListModel* createResourceFilterCheckListModel(QObject* parent);
    static AlarmListModel*    createAlarmListModel(QObject* parent);
    static AlarmListModel*    allAlarmListModel();
    static TemplateListModel* createTemplateListModel(QObject* parent);
    static TemplateListModel* allTemplateListModel();
};

// vim: et sw=4:
