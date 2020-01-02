/*
 *  datamodel.h  -  calendar data model dependent functions
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
class QObject;
class QWidget;

/** Class to create objects dependent on data model. */
namespace DataModel
{

void initialise();

/** Reload all resources' data from storage.
 *  @note In the case of Akonadi, this does not reload from the backend storage.
 */
void reload();

/** Reload a resource's data from storage.
 *  @note In the case of Akonadi, this does not reload from the backend storage.
 */
bool reload(Resource&);

bool isMigrationComplete();

/** Check for, and remove, any duplicate Akonadi resources, i.e. those which
 *  use the same calendar file/directory.
 */
void removeDuplicateResources();

ResourceListModel* createResourceListModel(QObject* parent);
ResourceFilterCheckListModel* createResourceFilterCheckListModel(QObject* parent);
AlarmListModel*    createAlarmListModel(QObject* parent);
AlarmListModel*    allAlarmListModel();
TemplateListModel* createTemplateListModel(QObject* parent);
TemplateListModel* allTemplateListModel();

ResourceCreator* createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent);

} // namespace DataModel

#endif // DATAMODEL_H

// vim: et sw=4:
