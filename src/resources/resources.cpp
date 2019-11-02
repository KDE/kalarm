/*
 *  resource.cpp  -  generic class containing an alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2019 David Jarvie <djarvie@kde.org>
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

#include "resources.h"

#include "resource.h"

Resources* Resources::mInstance = nullptr;

// Copy of all ResourceType instances with valid ID, wrapped in the Resource
// container which manages the instance.
QHash<ResourceId, Resource> Resources::mResources;

Resources* Resources::instance()
{
    if (!mInstance)
        mInstance = new Resources;
    return mInstance;
}

Resources::Resources()
{
}

Resource Resources::resource(ResourceId id)
{
    return mResources.value(id, Resource::null());
}

void Resources::removeResource(ResourceId id)
{
    mResources.remove(id);
}

void Resources::notifySettingsChanged(ResourceType* res, ResourceType::Changes change)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->settingsChanged(r, change);
    }
}

void Resources::notifyResourceMessage(ResourceType* res, ResourceType::MessageType type, const QString& message, const QString& details)
{
    if (res)
        notifyResourceMessage(res->id(), type, message, details);
}

void Resources::notifyResourceMessage(ResourceId id, ResourceType::MessageType type, const QString& message, const QString& details)
{
    Resource r = resource(id);
    if (r.isValid())
        Q_EMIT instance()->resourceMessage(r, type, message, details);
}

bool Resources::addResource(ResourceType* instance, Resource& resource)
{
    if (!instance  ||  instance->id() < 0)
    {
        // Instance is invalid - return an invalid resource.
        delete instance;
        resource = Resource::null();
        return false;
    }
    auto it = mResources.constFind(instance->id());
    if (it != mResources.constEnd())
    {
        // Instance ID already exists - return the existing resource.
        delete instance;
        resource = it.value();
        return false;
    }
    // Add a new resource.
    resource = Resource(instance);
    mResources[instance->id()] = resource;
    return true;
}

// vim: et sw=4:
