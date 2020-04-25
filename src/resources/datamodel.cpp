/*
 *  datamodel.cpp  -  model independent access to calendar functions
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#define USE_AKONADI
#ifdef USE_AKONADI
#include "akonadidatamodel.h"
#define DATA_MODEL AkonadiDataModel
#else
#include "fileresourcedatamodel.h"
#define DATA_MODEL FileResourceDataModel
#endif


void DataModel::initialise()
{
    DATA_MODEL::instance();
    // Record in kalarmrc, for information only, which backend is in use.
    Preferences::setBackend(ResourceDataModelBase::mInstance->dataStorageBackend());
    Preferences::self()->save();
}

void DataModel::terminate()
{
    if (ResourceDataModelBase::mInstance)
        ResourceDataModelBase::mInstance->terminate();
}

void DataModel::reload()
{
    if (ResourceDataModelBase::mInstance)
        ResourceDataModelBase::mInstance->reload();
}

bool DataModel::reload(Resource& resource)
{
    return ResourceDataModelBase::mInstance  &&  ResourceDataModelBase::mInstance->reload(resource);
}

bool DataModel::isMigrationComplete()
{
    return ResourceDataModelBase::mInstance  &&  ResourceDataModelBase::mInstance->isMigrationComplete();
}

void DataModel::removeDuplicateResources()
{
    if (ResourceDataModelBase::mInstance)
        ResourceDataModelBase::mInstance->removeDuplicateResources();
}

void DataModel::widgetNeedsDatabase(QWidget* widget)
{
    if (ResourceDataModelBase::mInstance)
        ResourceDataModelBase::mInstance->widgetNeedsDatabase(widget);
}

ResourceCreator* DataModel::createResourceCreator(KAlarmCal::CalEvent::Type defaultType, QWidget* parent)
{
    return ResourceDataModelBase::mInstance ? ResourceDataModelBase::mInstance->createResourceCreator(defaultType, parent) : nullptr;
}

void DataModel::updateCalendarToCurrentFormat(Resource& resource, bool ignoreKeepFormat, QObject* parent)
{
    if (ResourceDataModelBase::mInstance)
        ResourceDataModelBase::mInstance->updateCalendarToCurrentFormat(resource, ignoreKeepFormat, parent);
}

ResourceListModel* DataModel::createResourceListModel(QObject* parent)
{
    return ResourceDataModelBase::mInstance ? ResourceDataModelBase::mInstance->createResourceListModel(parent) : nullptr;
}

ResourceFilterCheckListModel* DataModel::createResourceFilterCheckListModel(QObject* parent)
{
    return ResourceDataModelBase::mInstance ? ResourceDataModelBase::mInstance->createResourceFilterCheckListModel(parent) : nullptr;
}

AlarmListModel* DataModel::createAlarmListModel(QObject* parent)
{
    return ResourceDataModelBase::mInstance ? ResourceDataModelBase::mInstance->createAlarmListModel(parent) : nullptr;
}

AlarmListModel* DataModel::allAlarmListModel()
{
    return ResourceDataModelBase::mInstance ? ResourceDataModelBase::mInstance->allAlarmListModel() : nullptr;
}

TemplateListModel* DataModel::createTemplateListModel(QObject* parent)
{
    return ResourceDataModelBase::mInstance ? ResourceDataModelBase::mInstance->createTemplateListModel(parent) : nullptr;
}

TemplateListModel* DataModel::allTemplateListModel()
{
    return ResourceDataModelBase::mInstance ? ResourceDataModelBase::mInstance->allTemplateListModel() : nullptr;
}

// vim: et sw=4:
