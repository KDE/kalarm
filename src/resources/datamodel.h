/*
 *  datamodel.h  -  model independent access to calendar functions
 *  Program:  kalarm
 *  Copyright © 2019-2020 David Jarvie <djarvie@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DATAMODEL_H
#define DATAMODEL_H

#include <KAlarmCal/KACalendar>

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

#if 0
    static QSize   iconSize()       { return mIconSize; }

    /** Return a bulleted list of alarm types for inclusion in an i18n message. */
    static QString typeListForDisplay(CalEvent::Types);

    /** Get the tooltip for a resource. The resource's enabled status is
     *  evaluated for specified alarm types. */
    QString tooltip(const Resource&, CalEvent::Types) const;

    /** Return the read-only status tooltip for a resource.
     * A null string is returned if the resource is fully writable. */
    static QString readOnlyTooltip(const Resource&);

    /** Return offset to add to headerData() role, for item models. */
    virtual int headerDataEventRoleOffset() const  { return 0; }
#endif
};

#endif // DATAMODEL_H

// vim: et sw=4:
