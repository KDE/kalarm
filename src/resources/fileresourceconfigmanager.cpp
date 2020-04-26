/*
 *  fileresourceconfigmanager.cpp  -  config manager for resources accessed via file system
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

#include "fileresourceconfigmanager.h"

#include "resources.h"
#include "singlefileresource.h"
#include "fileresourcecalendarupdater.h"
#include "preferences.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KLocalizedString>
#include <KConfig>
#include <KConfigGroup>

#include <QRegularExpression>

namespace
{
// Config file keys
const char* GROUP_GENERAL = "General";
const char* KEY_LASTID    = "LastId";
}

FileResourceConfigManager* FileResourceConfigManager::mInstance {nullptr};

/******************************************************************************
* Creates and returns the unique instance.
*/
FileResourceConfigManager* FileResourceConfigManager::instance()
{
    if (!mInstance)
        mInstance = new FileResourceConfigManager;
    return mInstance;
}

/******************************************************************************
* Constructor. Reads the current config and creates all resources.
*/
FileResourceConfigManager::FileResourceConfigManager()
{
    mConfig = new KConfig(QStringLiteral("kalarmresources"));
}

/******************************************************************************
* Read the current config and create all resources.
*/
void FileResourceConfigManager::createResources(QObject* parent)
{
    FileResourceConfigManager* manager = instance();
    if (manager->mCreated)
        return;
    manager->mCreated = 1;

    QStringList resourceGroups = manager->mConfig->groupList().filter(QRegularExpression(QStringLiteral("^Resource \\d+$")));
    if (!resourceGroups.isEmpty())
    {
        std::sort(resourceGroups.begin(), resourceGroups.end(),
                  [](const QString& g1, const QString& g2)
                  {
                      return g1.mid(9).toInt() < g2.mid(9).toInt();
                  });

        KConfigGroup general(manager->mConfig, GROUP_GENERAL);
        manager->mLastId = general.readEntry(KEY_LASTID, 0) | ResourceType::IdFlag;

        for (const QString& resourceGroup : resourceGroups)
        {
            int groupIndex = resourceGroup.mid(9).toInt();
            FileResourceSettings::Ptr settings(new FileResourceSettings(manager->mConfig, resourceGroup));
            if (!settings->isValid())
            {
                qCWarning(KALARM_LOG) << "FileResourceConfigManager: Invalid config for" << resourceGroup;
                manager->mConfig->deleteGroup(resourceGroup);   // invalid config for this resource
            }
            else
            {
                // Check for and remove duplicate URL or 'standard' setting
                for (auto it = manager->mResources.constBegin();  it != manager->mResources.constEnd();  ++it)
                {
                    const ResourceData& data = it.value();
                    if (settings->url() == data.resource.location())
                    {
                        qCWarning(KALARM_LOG) << "FileResourceConfigManager: Duplicate URL in config for" << resourceGroup;
                        manager->mConfig->deleteGroup(resourceGroup);   // invalid config for this resource
                        qCWarning(KALARM_LOG) << "FileResourceConfigManager: Deleted duplicate resource" << settings->displayName();
                        settings.clear();
                        break;
                    }
                    const CalEvent::Types std = settings->standardTypes() & data.settings->standardTypes();
                    if (std)
                    {
                        qCWarning(KALARM_LOG) << "FileResourceConfigManager: Duplicate 'standard' setting in config for" << resourceGroup;
                        settings->setStandard(settings->standardTypes() ^ std);
                    }
                }
                if (settings)
                {
                    Resource resource(createResource(settings));
                    manager->mResources[settings->id()] = ResourceData(resource, settings);
                    manager->mConfigGroups[groupIndex] = settings->id();

                    Resources::notifyNewResourceInitialised(resource);

                    // Update the calendar to the current KAlarm format if necessary, and
                    // if the user agrees.
                    FileResourceCalendarUpdater::updateToCurrentFormat(resource, false, parent);
                }
            }
        }

        // Allow any calendar updater instances to complete and auto-delete.
        FileResourceCalendarUpdater::waitForCompletion();
    }
    manager->mCreated = 2;
    Resources::notifyResourcesCreated();
}

/******************************************************************************
* Destructor. Writes the current config.
*/
FileResourceConfigManager::~FileResourceConfigManager()
{
    writeConfig();
    delete mConfig;
    mInstance = nullptr;
}

/******************************************************************************
* Writes the 'kalarmresources' config file.
*/
void FileResourceConfigManager::writeConfig()
{
    // No point in writing unless the config has already been read!
    if (mInstance)
        mInstance->mConfig->sync();
}

/******************************************************************************
* Return the IDs of all calendar resources.
*/
QList<ResourceId> FileResourceConfigManager::resourceIds()
{
    return instance()->mResources.keys();
}

/******************************************************************************
* Create a new calendar resource with the given settings.
*/
Resource FileResourceConfigManager::addResource(FileResourceSettings::Ptr& settings)
{
    // Find the first unused config group name index.
    FileResourceConfigManager* manager = instance();
    int lastIndex = 0;
    for (auto it = manager->mConfigGroups.constBegin();  it != manager->mConfigGroups.constEnd();  ++it)
    {
        int index = it.key();
        if (index > lastIndex + 1)
            break;
        lastIndex = index;
    }
    const int groupIndex = lastIndex + 1;

    // Get a unique ID.
    const int id = ++manager->mLastId;
    settings->setId(id);
    // Save the new last-used ID, but strip out IdFlag to make it more legible.
    KConfigGroup general(manager->mConfig, GROUP_GENERAL);
    general.writeEntry(KEY_LASTID, id & ~ResourceType::IdFlag);

    const QString configGroup = groupName(groupIndex);
    settings->createConfig(manager->mConfig, configGroup);
    manager->mConfigGroups[groupIndex] = id;
    Resource resource(createResource(settings));
    manager->mResources[id] = ResourceData(resource, settings);

    Resources::notifyNewResourceInitialised(resource);
    return resource;
}

/******************************************************************************
* Delete a calendar resource and its settings.
*/
bool FileResourceConfigManager::removeResource(Resource& resource)
{
    if (resource.isValid())
    {
        FileResourceConfigManager* manager = instance();
        const ResourceId id = resource.id();
        const int groupIndex = manager->findResourceGroup(id);
        if (groupIndex >= 0)
        {
            const QString configGroup = groupName(groupIndex);
            manager->mConfig->deleteGroup(configGroup);
            manager->mConfigGroups.remove(groupIndex);
            manager->mResources.remove(id);
            return true;
        }
    }
    return false;
}

/******************************************************************************
* Return the available file system resource types handled by the manager.
*/
QList<ResourceType::StorageType> FileResourceConfigManager::storageTypes()
{
    return { ResourceType::File
//           , ResourceType::Directory   // not currently intended to be implemented
           };
}

/******************************************************************************
* Find the config group for a resource ID.
*/
int FileResourceConfigManager::findResourceGroup(ResourceId id) const
{
    for (auto it = mConfigGroups.constBegin();  it != mConfigGroups.constEnd();  ++it)
        if (it.value() == id)
            return it.key();
    return -1;
}

/******************************************************************************
* Return the config group name for a given config group index.
*/
QString FileResourceConfigManager::groupName(int groupIndex)
{
    return QStringLiteral("Resource %1").arg(groupIndex);
}

/******************************************************************************
* Create a new resource with the given settings.
*/
Resource FileResourceConfigManager::createResource(FileResourceSettings::Ptr& settings)
{
    switch (settings->storageType())
    {
        case FileResourceSettings::File:
            return SingleFileResource::create(settings.data());
        case FileResourceSettings::Directory:   // not currently intended to be implemented
        default:
            return Resource::null();
    }
}

// vim: et sw=4:
