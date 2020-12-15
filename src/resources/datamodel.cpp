/*
 *  datamodel.cpp  -  model independent access to calendar functions
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "datamodel.h"

#include "config-kalarm.h"
#if FILE_RESOURCES
#include "fileresourcedatamodel.h"
#define DATA_MODEL FileResourceDataModel
#else
#include "akonadidatamodel.h"
#define DATA_MODEL AkonadiDataModel
#error This option is now unmaintained
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
