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
#include "kalarm_debug.h"

#include <QEventLoop>
#include <QTimer>

Resources* Resources::mInstance{nullptr};

// Copy of all ResourceType instances with valid ID, wrapped in the Resource
// container which manages the instance.
QHash<ResourceId, Resource> Resources::mResources;

QEventLoop*                 Resources::mPopulatedCheckLoop{nullptr};
bool                        Resources::mCreated{false};
bool                        Resources::mPopulated{false};


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
    if (mResources.remove(id) > 0)
        Q_EMIT instance()->resourceRemoved(id);
}

/******************************************************************************
* Return the enabled resources which contain a specified alarm type.
* If 'writable' is true, only writable resources are included.
*/
QVector<Resource> Resources::enabledResources(CalEvent::Type type, bool writable)
{
    QVector<Resource> result;
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (writable  &&  !res.isWritable())
            continue;
        if (res.alarmTypes() & type)
            result += res;
    }
    return result;
}

/******************************************************************************
* Return the standard resource for an alarm type.
*/
Resource Resources::getStandard(CalEvent::Type type)
{
    Resources* manager = instance();
    bool wantDefaultArchived = (type == CalEvent::ARCHIVED);
    Resource defaultArchived;
    for (auto it = manager->mResources.constBegin();  it != manager->mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.isWritable(type))
        {
            if (res.configIsStandard(type))
                return res;
            if (wantDefaultArchived)
            {
                if (defaultArchived.isValid())
                    wantDefaultArchived = false;   // found two archived alarm resources
                else
                    defaultArchived = res;   // this is the first archived alarm resource
            }
        }
    }

    if (wantDefaultArchived  &&  defaultArchived.isValid())
    {
        // There is no resource specified as the standard archived alarm
        // resource, but there is exactly one writable archived alarm
        // resource. Set that resource to be the standard.
        defaultArchived.configSetStandard(CalEvent::ARCHIVED, true);
        return defaultArchived;
    }

    return Resource();
}

/******************************************************************************
* Return whether a collection is the standard collection for a specified
* mime type.
*/
bool Resources::isStandard(const Resource& resource, CalEvent::Type type)
{
    // If it's for archived alarms, get and also set the standard resource if
    // necessary.
    if (type == CalEvent::ARCHIVED)
        return getStandard(type) == resource;

    return resource.configIsStandard(type) && resource.isWritable(type);
}

/******************************************************************************
* Return the alarm types for which a resource is the standard resource.
*/
CalEvent::Types Resources::standardTypes(const Resource& resource, bool useDefault)
{
    if (!resource.isWritable())
        return CalEvent::EMPTY;

    Resources* manager = instance();
    auto it = manager->mResources.constFind(resource.id());
    if (it == manager->mResources.constEnd())
        return CalEvent::EMPTY;
    CalEvent::Types stdTypes = resource.configStandardTypes() & resource.enabledTypes();
    if (useDefault)
    {
        // Also return alarm types for which this is the only resource.
        // Check if it is the only writable resource for these type(s).

        if (!(stdTypes & CalEvent::ARCHIVED)  &&  resource.isEnabled(CalEvent::ARCHIVED))
        {
            // If it's the only enabled archived alarm resource, set it as standard.
            getStandard(CalEvent::ARCHIVED);
            stdTypes = resource.configStandardTypes() & resource.enabledTypes();
        }
        CalEvent::Types enabledNotStd = resource.enabledTypes() & ~stdTypes;
        if (enabledNotStd)
        {
            // The resource is enabled for type(s) for which it is not the standard.
            for (auto itr = manager->mResources.constBegin();  itr != manager->mResources.constEnd() && enabledNotStd;  ++itr)
            {
                const Resource& res = itr.value();
                if (res != resource  &&  res.isWritable())
                {
                    const CalEvent::Types en = res.enabledTypes() & enabledNotStd;
                    if (en)
                        enabledNotStd &= ~en;   // this resource handles the same alarm type
                }
            }
        }
        stdTypes |= enabledNotStd;
    }
    return stdTypes;
}

/******************************************************************************
* Set or clear the standard status for a resource.
*/
void Resources::setStandard(Resource& resource, CalEvent::Type type, bool standard)
{
    if (!(type & resource.enabledTypes()))
        return;

    Resources* manager = instance();
    auto it = manager->mResources.find(resource.id());
    if (it == manager->mResources.end())
        return;
    resource = it.value();   // just in case it's a different object!
    if (standard == resource.configIsStandard(type))
        return;

    if (!standard)
        resource.configSetStandard(type, false);
    else if (resource.isWritable(type))
    {
        // Clear the standard status for any other resources.
        for (auto itr = manager->mResources.begin();  itr != manager->mResources.end();  ++itr)
        {
            Resource& res = itr.value();
            if (res != resource)
                res.configSetStandard(type, false);
        }
        resource.configSetStandard(type, true);
    }
}

/******************************************************************************
* Set the alarm types for which a resource the standard resource.
*/
void Resources::setStandard(Resource& resource, CalEvent::Types types)
{
    types &= resource.enabledTypes();

    Resources* manager = instance();
    auto it = manager->mResources.find(resource.id());
    if (it == manager->mResources.end())
        return;
    resource = it.value();   // just in case it's a different object!
    if (types != resource.configStandardTypes()
    &&  (!types  ||  resource.isWritable()))
    {
        if (types)
        {
            // Clear the standard status for any other resources.
            for (auto itr = manager->mResources.begin();  itr != manager->mResources.end();  ++itr)
            {
                Resource& res = itr.value();
                if (res != resource)
                {
                    const CalEvent::Types rtypes = res.configStandardTypes();
                    if (rtypes & types)
                        res.configSetStandard(rtypes & ~types);
                }
            }
        }
        resource.configSetStandard(types);
    }
}

/******************************************************************************
* Wait for one or all enabled resources to be populated.
* Reply = true if successful.
*/
bool Resources::waitUntilPopulated(ResourceId id, int timeout)
{
    qCDebug(KALARM_LOG) << "Resources::waitUntilPopulated" << id;
    Resources* manager = instance();
    int result = 1;
    while (!manager->mCreated  ||  !isLoaded(id))
    {
        if (!mPopulatedCheckLoop)
            mPopulatedCheckLoop = new QEventLoop(instance());
        if (timeout > 0)
            QTimer::singleShot(timeout * 1000, mPopulatedCheckLoop, &QEventLoop::quit);
        result = mPopulatedCheckLoop->exec();
    }
    delete mPopulatedCheckLoop;
    mPopulatedCheckLoop = nullptr;
    return result;
}

/******************************************************************************
* Called when all configured resources have been created for the first time.
*/
void Resources::notifyResourcesCreated()
{
    mCreated = true;
    checkResourcesPopulated();
}

/******************************************************************************
* Called when a collection has been populated.
* Emits a signal if all collections have been populated.
*/
void Resources::notifyResourceLoaded(const ResourceType* res)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->resourceLoaded(r);
    }

    // Check whether all resources have now loaded at least once.
    checkResourcesPopulated();
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

/******************************************************************************
* To be called when a resource has been created or loaded.
* If all resources have now loaded for the first time, emit signal.
*/
void Resources::checkResourcesPopulated()
{
    // Exit from the populated event loop to allow waitUntilPopulated() to complete.
    if (mPopulatedCheckLoop)
        mPopulatedCheckLoop->exit(1);

    if (!mPopulated  &&  mCreated)
    {
        // Check whether all resources have now loaded at least once.
        for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
        {
            if (!it.value().isLoaded())
                return;
        }
        mPopulated = true;
        Q_EMIT instance()->resourcesPopulated();
    }
}

/******************************************************************************
* Return whether one or all enabled collections have been loaded.
*/
bool Resources::isLoaded(ResourceId id)
{
    if (id >= 0)
    {
        const Resource res = resource(id);
        return res.isLoaded()
           ||  res.enabledTypes() == CalEvent::EMPTY;
    }

    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (!res.isLoaded()
        &&  res.enabledTypes() != CalEvent::EMPTY)
            return false;
    }
    return true;
}

// vim: et sw=4:
