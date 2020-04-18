/*
 *  datamodel.cpp  -  calendar data model dependent functions
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

#include "datamodel.h"

#include "akonadidatamodel.h"
#include "akonadiresource.h"
#include "akonadiresourcecreator.h"
#include "akonadicalendarupdater.h"
#include "eventmodel.h"
#include "resourcemodel.h"

namespace DataModel
{

void initialise()
{
    AkonadiDataModel* model = AkonadiDataModel::instance();
    Preferences::setBackend(model->dataStorageBackend());
    Preferences::self()->save();
}

void terminate()
{
}

void widgetNeedsDatabase(QWidget* widget)
{
    Akonadi::ControlGui::widgetNeedsAkonadi(widget);
}

void reload()
{
    AkonadiDataModel::instance()->reload();
}

bool reload(Resource& resource)
{
    return AkonadiDataModel::instance()->reload(resource);
}

bool isMigrationComplete()
{
    return AkonadiDataModel::instance()->isMigrationComplete();
}

void removeDuplicateResources()
{
    AkonadiResource::removeDuplicateResources();
}

ResourceListModel* createResourceListModel(QObject* parent)
{
    return ResourceListModel::create<AkonadiDataModel>(parent);
}

ResourceFilterCheckListModel* createResourceFilterCheckListModel(QObject* parent)
{
    return ResourceFilterCheckListModel::create<AkonadiDataModel>(parent);
}

AlarmListModel* createAlarmListModel(QObject* parent)
{
    return AlarmListModel::create<AkonadiDataModel>(parent);
}

AlarmListModel* allAlarmListModel()
{
    return AlarmListModel::all<AkonadiDataModel>();
}

TemplateListModel* createTemplateListModel(QObject* parent)
{
    return TemplateListModel::create<AkonadiDataModel>(parent);
}

TemplateListModel* allTemplateListModel()
{
    return TemplateListModel::all<AkonadiDataModel>();
}

ResourceCreator* createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent)
{
    return new AkonadiResourceCreator(defaultType, parent);
}

void updateCalendarToCurrentFormat(Resource& resource, bool ignoreKeepFormat, QObject* parent)
{
    AkonadiCalendarUpdater::updateToCurrentFormat(resource, ignoreKeepFormat, parent);
}

} // namespace DataModel

// vim: et sw=4:
