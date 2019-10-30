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

Resources* Resources::instance()
{
    if (!mInstance)
        mInstance = new Resources;
    return mInstance;
}

Resources::Resources()
{
}

ResourceType* Resources::resource(ResourceId id)
{
    return ResourceType::mInstances.value(id, nullptr);
}

void Resources::notifySettingsChanged(ResourceType* resource, ResourceType::Changes change)
{
    if (resource)
        Q_EMIT instance()->settingsChanged(resource->id(), change);
}

void Resources::notifyResourceMessage(ResourceType* resource, ResourceType::MessageType type, const QString& message, const QString& details)
{
    if (resource)
        Q_EMIT instance()->resourceMessage(resource->id(), type, message, details);
}

// vim: et sw=4:
