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
#include "resourcedatamodelbase.h"
#include "resourcemodel.h"
#include "resourceselectdialog.h"
#include "preferences.h"
#include "lib/autoqpointer.h"
#include "kalarm_debug.h"

#include <KLocalizedString>

Resources* Resources::mInstance{nullptr};

// Copy of all ResourceType instances with valid ID, wrapped in the Resource
// container which manages the instance.
QHash<ResourceId, Resource> Resources::mResources;

bool Resources::mCreated{false};
bool Resources::mPopulated{false};


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

/******************************************************************************
* Return the resources which are enabled for a specified alarm type.
* If 'writable' is true, only writable resources are included.
*/
QVector<Resource> Resources::enabledResources(CalEvent::Type type, bool writable)
{
    const CalEvent::Types types = (type == CalEvent::EMPTY)
                                ? CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE
                                : type;

    QVector<Resource> result;
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (writable  &&  !res.isWritable())
            continue;
        if (res.enabledTypes() & types)
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
* Get the collection to use for storing an alarm.
* Optionally, the standard collection for the alarm type is returned. If more
* than one collection is a candidate, the user is prompted.
*/
Resource Resources::destination(ResourceListModel* model, CalEvent::Type type, QWidget* promptParent, bool noPrompt, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Resource standard;
    if (type == CalEvent::EMPTY)
        return standard;
    standard = getStandard(type);
    // Archived alarms are always saved in the default resource,
    // else only prompt if necessary.
    if (type == CalEvent::ARCHIVED  ||  noPrompt
    ||  (!Preferences::askResource()  &&  standard.isValid()))
        return standard;

    // Prompt for which collection to use
    model->setFilterWritable(true);
    model->setFilterEnabled(true);
    model->setEventTypeFilter(type);
    model->useResourceColour(false);
    Resource res;
    switch (model->rowCount())
    {
        case 0:
            break;
        case 1:
            res = model->resource(0);
            break;
        default:
        {
            // Use AutoQPointer to guard against crash on application exit while
            // the dialogue is still open. It prevents double deletion (both on
            // deletion of 'promptParent', and on return from this function).
            AutoQPointer<ResourceSelectDialog> dlg = new ResourceSelectDialog(model, promptParent);
            dlg->setWindowTitle(i18nc("@title:window", "Choose Calendar"));
            dlg->setDefaultResource(standard);
            if (dlg->exec())
                res = dlg->selectedResource();
            if (!res.isValid()  &&  cancelled)
                *cancelled = true;
        }
    }
    return res;
}

/******************************************************************************
* Return whether all configured resources have been created.
*/
bool Resources::allCreated()
{
    return instance()->mCreated;
}

/******************************************************************************
* Return whether all configured resources have been loaded at least once.
*/
bool Resources::allPopulated()
{
    return instance()->mPopulated;
}

/******************************************************************************
* Return the resource which an event belongs to, provided its alarm type is
* enabled.
*/
Resource Resources::resourceForEvent(const QString& eventId)
{
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.containsEvent(eventId))
            return res;
    }
    return Resource::null();
}

/******************************************************************************
* Return the resource which an event belongs to, and the event, provided its
* alarm type is enabled.
*/
Resource Resources::resourceForEvent(const QString& eventId, KAEvent& event)
{
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        event = res.event(eventId);
        if (event.isValid())
            return res;
    }
    if (mResources.isEmpty())   // otherwise, 'event' was set invalid in the loop
        event = KAEvent();
    return Resource::null();
}

/******************************************************************************
* Return the resource which has a given configuration identifier.
*/
Resource Resources::resourceForConfigName(const QString& configName)
{
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.configName() == configName)
            return res;
    }
    return Resource::null();
}

/******************************************************************************
* Called after a new resource has been created, when it has completed its
* initialisation.
*/
void Resources::notifyNewResourceInitialised(Resource& res)
{
    if (res.isValid())
        Q_EMIT instance()->resourceAdded(res);
}

/******************************************************************************
* Called when all configured resources have been created for the first time.
*/
void Resources::notifyResourcesCreated()
{
    mCreated = true;
    Q_EMIT instance()->resourcesCreated();
    checkResourcesPopulated();
}

/******************************************************************************
* Called when a resource's events have been loaded.
* Emits a signal if all collections have been populated.
*/
void Resources::notifyResourcePopulated(const ResourceType* res)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->resourcePopulated(r);
    }

    // Check whether all resources have now loaded at least once.
    checkResourcesPopulated();
}

/******************************************************************************
* Called by a resource to notify that its settings have changed.
* Emits the settingsChanged() signal.
* If the resource is now read-only and was standard, clear its standard status.
* If the resource has newly enabled alarm types, ensure that it doesn't
* duplicate any existing standard setting.
*/
void Resources::notifySettingsChanged(ResourceType* res, ResourceType::Changes change, CalEvent::Types oldEnabled)
{
    if (!res)
        return;
    Resource r = resource(res->id());
    if (!r.isValid())
        return;

    Resources* manager = instance();

    if (change & ResourceType::Enabled)
    {
        ResourceType::Changes change = ResourceType::Enabled;

        // Find which alarm types (if any) have been newly enabled.
        const CalEvent::Types extra    = res->enabledTypes() & ~oldEnabled;
        CalEvent::Types       std      = res->configStandardTypes();
        const CalEvent::Types extraStd = std & extra;
        if (extraStd  &&  res->isWritable())
        {
            // Alarm type(s) have been newly enabled, and are set as standard.
            // Don't allow the resource to be set as standard for those types if
            // another resource is already the standard.
            CalEvent::Types disallowedStdTypes{};
            for (auto it = manager->mResources.constBegin();  it != manager->mResources.constEnd();  ++it)
            {
                const Resource& resit = it.value();
                if (resit.id() != res->id()  &&  resit.isWritable())
                {
                    disallowedStdTypes |= extraStd & resit.configStandardTypes() & resit.enabledTypes();
                    if (extraStd == disallowedStdTypes)
                        break;   // all the resource's newly enabled standard types are disallowed
                }
            }
            if (disallowedStdTypes)
            {
                std &= ~disallowedStdTypes;
                res->configSetStandard(std);
            }
        }
        if (std)
            change |= ResourceType::Standard;
    }

    Q_EMIT manager->settingsChanged(r, change);

    if ((change & ResourceType::ReadOnly)  &&  res->readOnly())
    {
        qCDebug(KALARM_LOG) << "Resources::notifySettingsChanged:" << res->id() << "ReadOnly";
        // A read-only resource can't be the default for any alarm type
        const CalEvent::Types std = standardTypes(r, false);
        if (std != CalEvent::EMPTY)
        {
            setStandard(r, CalEvent::EMPTY);
            bool singleType = true;
            QString msg;
            switch (std)
            {
                case CalEvent::ACTIVE:
                    msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for active alarms.",
                            res->displayName());
                    break;
                case CalEvent::ARCHIVED:
                    msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for archived alarms.",
                            res->displayName());
                    break;
                case CalEvent::TEMPLATE:
                    msg = xi18n("The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for alarm templates.",
                            res->displayName());
                    break;
                default:
                    msg = xi18nc("@info", "<para>The calendar <resource>%1</resource> has been made read-only. "
                            "This was the default calendar for:%2</para>"
                            "<para>Please select new default calendars.</para>",
                            res->displayName(), ResourceDataModelBase::typeListForDisplay(std));
                    singleType = false;
                    break;
            }
            if (singleType)
                msg = xi18nc("@info", "<para>%1</para><para>Please select a new default calendar.</para>", msg);
            notifyResourceMessage(res->id(), ResourceType::MessageType::Info, msg, QString());
        }
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

void Resources::notifyEventsAdded(ResourceType* res, const QList<KAEvent>& events)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->eventsAdded(r, events);
    }
}

void Resources::notifyEventUpdated(ResourceType* res, const KAEvent& event)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->eventUpdated(r, event);
    }
}

void Resources::notifyEventsToBeRemoved(ResourceType* res, const QList<KAEvent>& events)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->eventsToBeRemoved(r, events);
    }
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

void Resources::removeResource(ResourceId id)
{
    if (mResources.remove(id) > 0)
        Q_EMIT instance()->resourceRemoved(id);
}

/******************************************************************************
* To be called when a resource has been created or loaded.
* If all resources have now loaded for the first time, emit signal.
*/
void Resources::checkResourcesPopulated()
{
    if (!mPopulated  &&  mCreated)
    {
        // Check whether all resources have now loaded at least once.
        for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
        {
            if (!it.value().isPopulated())
                return;
        }
        mPopulated = true;
        Q_EMIT instance()->resourcesPopulated();
    }
}

#if 0
/******************************************************************************
* Return whether one or all enabled collections have been loaded.
*/
bool Resources::isPopulated(ResourceId id)
{
    if (id >= 0)
    {
        const Resource res = resource(id);
        return res.isPopulated()
           ||  res.enabledTypes() == CalEvent::EMPTY;
    }

    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (!res.isPopulated()
        &&  res.enabledTypes() != CalEvent::EMPTY)
            return false;
    }
    return true;
}
#endif

// vim: et sw=4:
