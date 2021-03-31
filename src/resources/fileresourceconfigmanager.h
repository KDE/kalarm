/*
 *  fileresourceconfigmanager.h  -  config manager for resources accessed via file system
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "resource.h"
#include "fileresource.h"
#include "fileresourcesettings.h"

#include <QSharedPointer>

class KConfig;

/** Manager for configuration files for file system resources.
 *  Reads configuration files and creates resources at startup, and updates
 *  configuration files when resource configurations change.
 */
class FileResourceConfigManager
{
public:
    /** Returns the unique instance, and creates it if necessary.
     *  Call createResources() to read the resource configuration and create
     *  the resources defined in it.
     */
    static FileResourceConfigManager* instance();

    /** Destructor.
     *  Writes the 'kalarmresources' config file.
     */
    ~FileResourceConfigManager();

    /** Reads the 'kalarmresources' config file and creates the resources
     *  defined in it. If called more than once, this method will do nothing.
     */
    static void createResources(QObject* parent);

    /** Writes the 'kalarmresources' config file. */
    static void writeConfig();

    /** Return the IDs of all file system calendar resources. */
    static QList<ResourceId> resourceIds();

    /** Create a new file system calendar resource with the given settings.
     *  Use writeConfig() to write the updated config file.
     *  @param settings  The resource's configuration; updated with the unique
     *                   ID for the resource.
     */
    static Resource addResource(FileResourceSettings::Ptr& settings);

    /** Delete a specified file system calendar resource and its settings.
     *  The calendar file is not removed.
     *  Use writeConfig() to write the updated config file.
     *  To be called only from FileResource, which will delete the resource
     *  from Resources.
     */
    static bool removeResource(Resource&);

    /** Return the available file system resource types handled by the manager. */
    static QList<ResourceType::StorageType> storageTypes();

private:
    FileResourceConfigManager();
    int findResourceGroup(ResourceId id) const;
    static QString groupName(int groupIndex);
    static Resource createResource(FileResourceSettings::Ptr&);

    struct ResourceData
    {
        Resource                 resource;
        FileResourceSettings::Ptr settings;

        ResourceData() {}
        ResourceData(Resource r, FileResourceSettings::Ptr s) : resource(r), settings(s) {}
        ResourceData(const ResourceData&) = default;
        ResourceData& operator=(const ResourceData&) = default;
    };
    static FileResourceConfigManager* mInstance;
    KConfig*                        mConfig;
    QHash<ResourceId, ResourceData> mResources;      // resource ID, resource & its settings
    QMap<int, ResourceId>           mConfigGroups;   // group name index, resource ID
    ResourceId                      mLastId {0};     // last ID which was allocated to any resource
    int                             mCreated {0};    // 1 = createResources() has been run, 2 = completed
};


// vim: et sw=4:
