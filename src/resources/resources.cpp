/*
 *  resource.cpp  -  generic class containing an alarm calendar resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "resources.h"

#include "resourcedatamodelbase.h"
#include "resourceselectdialog.h"

#include "preferences.h"
#include "lib/autoqpointer.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KLocalizedString>


Resources* Resources::mInstance {nullptr};

// Copy of all ResourceType instances with valid ID, wrapped in the Resource
// container which manages the instance.
QHash<ResourceId, Resource> Resources::mResources;

bool Resources::mCreated {false};
bool Resources::mPopulated {false};


Resources* Resources::instance()
{
    if (!mInstance)
        mInstance = new Resources;
    return mInstance;
}

Resources::Resources()
{
    qRegisterMetaType<ResourceType::MessageType>();
}

Resources::~Resources()
{
    qCDebug(KALARM_LOG) << "Resources::~Resources";
    for (auto it = mResources.begin();  it != mResources.end();  ++it)
        it.value().close();
}

Resource Resources::resource(ResourceId id)
{
    return mResources.value(id, Resource::null());
}

Resource Resources::resourceFromDisplayId(ResourceId id)
{
    Resource result = Resource::null();
    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.displayId() == id)
        {
            if (result != Resource::null())
                return Resource::null();
            result = res;
        }
    }
    return result;
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
Resource Resources::getStandard(CalEvent::Type type, bool useOnlyResource)
{
    Resources* manager = instance();
    Resource defaultResource;
    for (auto it = manager->mResources.constBegin();  it != manager->mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res.isWritable(type))
        {
            if (res.configIsStandard(type))
                return res;
            if (useOnlyResource)
            {
                if (defaultResource.isValid())
                    useOnlyResource = false;   // found two resources for the type
                else
                    defaultResource = res;   // this is the first resource for the type
            }
        }
    }

    if (useOnlyResource  &&  defaultResource.isValid())
    {
        // There is no resource specified as the standard resource for the
        // alarm type, but there is exactly one writable reseource for the
        // type. Set that resource to be the standard.
        defaultResource.configSetStandard(type, true);
        return defaultResource;
    }

    return {};
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
        return getStandard(type, true) == resource;

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
            getStandard(CalEvent::ARCHIVED, true);
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
    auto it = manager->mResources.constFind(resource.id());
    if (it == manager->mResources.constEnd())
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
    auto it = manager->mResources.constFind(resource.id());
    if (it == manager->mResources.constEnd())
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
* Find the resource to be used to store an event of a given type.
* This will be the standard resource for the type, but if this is not valid,
* the user will be prompted to select a resource.
*/
Resource Resources::destination(CalEvent::Type type, QWidget* promptParent, DestOptions options, bool* cancelled)
{
    if (cancelled)
        *cancelled = false;
    Resource standard;
    if (type == CalEvent::EMPTY)
        return standard;
    standard = getStandard(type, (options & UseOnlyResource));
    // Archived alarms are always saved in the default resource,
    // else only prompt if necessary.
    if (type == CalEvent::ARCHIVED  ||  (options & NoResourcePrompt)
    ||  (!Preferences::askResource()  &&  standard.isValid()))
        return standard;

    // Prompt for which collection to use
    ResourceListModel* model = DataModel::createResourceListModel(promptParent);
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
* Called when the user changes the start-of-day time.
* Adjust the start times of all date-only alarms' recurrences.
*/
void Resources::adjustStartOfDay()
{
    for (auto it = mResources.begin();  it != mResources.end();  ++it)
        it.value().adjustStartOfDay();
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
    qCDebug(KALARM_LOG) << "Resources::notifyResourcesCreated";
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
* Called to notify that a resource is about to be removed.
*/
void Resources::notifyResourceToBeRemoved(ResourceType* res)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->resourceToBeRemoved(r);
    }
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
        if (res->enabledTypes() == oldEnabled)
            change &= ~ResourceType::Enabled;
    }

    Q_EMIT manager->settingsChanged(r, change);

    if ((change & ResourceType::ReadOnly)  &&  res->readOnly())
    {
        qCDebug(KALARM_LOG) << "Resources::notifySettingsChanged:" << res->displayId() << "ReadOnly";
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
                    msg = xi18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                 "This was the default calendar for active alarms.",
                                 res->displayName());
                    break;
                case CalEvent::ARCHIVED:
                    msg = xi18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
                                 "This was the default calendar for archived alarms.",
                                 res->displayName());
                    break;
                case CalEvent::TEMPLATE:
                    msg = xi18nc("@info", "The calendar <resource>%1</resource> has been made read-only. "
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
    if (resource(id).isValid())
        Q_EMIT instance()->resourceMessage(type, message, details);
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

void Resources::notifyEventsRemoved(ResourceType* res, const QList<KAEvent>& events)
{
    if (res)
    {
        Resource r = resource(res->id());
        if (r.isValid())
            Q_EMIT instance()->eventsRemoved(r, events);
    }
}

void Resources::notifySettingsDestroyed(ResourceId id)
{
    Resource r = resource(id);
    if (r.isValid())
        r.removeSettings();
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
            const Resource& res = it.value();
            if (res.isEnabled(CalEvent::EMPTY)  &&  !res.isPopulated())
                return;
        }
        mPopulated = true;
        qCDebug(KALARM_LOG) << "Resources::checkResourcesPopulated: emit signal";
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
