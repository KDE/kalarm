/*
 *  resources.h  -  container for all ResourceType instances
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "datamodel.h"
#include "resource.h"
#include "resourcemodel.h"

#include <QObject>

using namespace KAlarmCal;

/** Class to contain all ResourceType instances.
 *  It provides connection to signals from all ResourceType instances.
 */
class Resources : public QObject
{
    Q_OBJECT
public:
    /** Creates the unique Resources instance.
     *  Note that this merely creates a container for individual resources,
     *  and doesn't create or initialise any ResourceType instances.
     */
    static Resources* instance();

    ~Resources() override;
    Resources(const Resources&) = delete;
    Resources& operator=(const Resources&) const = delete;

    /** Return a copy of the resource with a given ID.
     *  @return  The resource, or invalid if the ID doesn't already exist or is invalid.
     */
    static Resource resource(ResourceId);

    /** Return a copy of the resource with a given display ID.
     *  @return  The resource, or invalid if the ID doesn't already exist or is invalid
     *           or if more than one resource has the same display ID.
     */
    static Resource resourceFromDisplayId(ResourceId);

    /** Remove a resource. The calendar file is not removed.
     *  @return true if the resource has been removed or a removal job has been scheduled.
     */
    static bool removeResource(Resource&);

    /** Sorting criteria for allResources(Type, Sorting). May be OR'ed together. */
    enum Sorts
    {
        NoSort       = 0,
        DisplayName  = 0x01,   // sort by display name
        DefaultFirst = 0x02    // default resource is first in list. Requires a CalEvent::Type to be specified.
    };
    Q_DECLARE_FLAGS(Sorting, Sorts)
    /** Return all resources of a kind which contain a specified alarm type.
     *  @tparam RType      Resource type to fetch, default = all types.
     *  @param  alarmType  Alarm type to check for, or CalEvent::EMPTY for any type.
     *  @param  sorting    Sorting criteria to use.
     */
    template <class RType = ResourceType>
    static QVector<Resource> allResources(CalEvent::Type alarmType = CalEvent::EMPTY, Sorting sorting = NoSort);

    /** Return the enabled resources which contain a specified alarm type.
     *  @param type      Alarm type to check for, or CalEvent::EMPTY for any type.
     *  @param writable  If true, only writable resources are included.
     */
    static QVector<Resource> enabledResources(CalEvent::Type type = CalEvent::EMPTY, bool writable = false);

    /** Return the standard resource for an alarm type. This is the resource
     *  which can be set as the default to add new alarms to.
     *  Only enabled and writable resources can be standard.
     *  In the case of archived alarm resources, if no resource is specified
     *  as standard and there is exactly one writable archived alarm resource,
     *  that resource will be automatically set as standard.
     *
     *  @param type             Alarm type.
     *  @param useOnlyResource  If there is only one resource for the alarm type,
     *                          set it as standard.
     *  @return standard resource, or null if none.
     */
    static Resource getStandard(CalEvent::Type type, bool useOnlyResource = false);

    /** Return whether a resource is the standard resource for a specified alarm
     *  type. Only enabled and writable resources can be standard.
     *  In the case of archived alarms, if no resource is specified as standard
     *  and the resource is the only writable archived alarm resource, it will
     *  be automatically set as standard.
     */
    static bool isStandard(const Resource& resource, CalEvent::Type);

    /** Return the alarm type(s) for which a resource is the standard resource.
     *  Only enabled and writable resources can be standard.
     *  @param useDefault false to return the defined standard types, if any;
     *                    true to return the types for which it is the standard
     *                    or only resource.
     */
    static CalEvent::Types standardTypes(const Resource& resource, bool useDefault = false);

    /** Set or clear a resource as the standard resource for a specified alarm
     *  type. This does not affect its status for other alarm types.
     *  The resource must be writable and enabled for the type, to set
     *  standard = true.
     *  If the resource is being set as standard, the standard status for the
     *  alarm type is cleared for any other resources.
     */
    static void setStandard(Resource& resource, CalEvent::Type, bool standard);

    /** Set which alarm types a resource is the standard resource for.
     *  Its standard status is cleared for other alarm types.
     *  The resource must be writable and enabled for the type, to set
     *  standard = true.
     *  If the resource is being set as standard for any alarm types, the
     *  standard status is cleared for those alarm types for any other resources.
     */
    static void setStandard(Resource& resource, CalEvent::Types);

    /** Options for destination(). May be OR'ed together. */
    enum DestOption
    {
        NoDestOption     = 0,
        NoResourcePrompt = 0x01,   //!< Don't prompt the user even if the standard resource is not valid.
        UseOnlyResource  = 0x02    //!< If there is only one enabled resource, set it as standard.
    };
    Q_DECLARE_FLAGS(DestOptions, DestOption)

    /** Find the resource to be used to store an event of a given type.
     *  This will be the standard resource for the type, but if this is not valid,
     *  the user will be prompted to select a resource.
     *  @param type         The event type
     *  @param promptParent The parent widget for the prompt
     *  @param options      Options to use
     *  @param cancelled    If non-null: set to true if the user cancelled the
     *                      prompt dialogue; set to false if any other error
     */
    static Resource destination(CalEvent::Type type, QWidget* promptParent = nullptr, DestOptions options = NoDestOption, bool* cancelled = nullptr);

    /** Return whether all configured and migrated resources have been created. */
    static bool allCreated();

    /** Return whether all configured and migrated resources have been loaded
     *  at least once. */
    static bool allPopulated();

    /** Return the resource which an event belongs to, provided that the event's
     *  alarm type is enabled. */
    static Resource resourceForEvent(const QString& eventId);

    /** Return the resource which an event belongs to, and the event, provided
     *  that the event's alarm type is enabled. */
    static Resource resourceForEvent(const QString& eventId, KAEvent& event);

    /** Return the resource which has a given configuration identifier. */
    static Resource resourceForConfigName(const QString& configName);

    /** To be called when the start-of-day time has changed, to adjust the start
     *  times of all date-only alarms' recurrences, in all resources.
     */
    static void adjustStartOfDay();

    /** Called to notify that a new resource has completed its initialisation,
     *  in order to emit the resourceAdded() signal. */
    static void notifyNewResourceInitialised(Resource&);

    /** Called to notify that all configured and migrated resources have now
     *  been created. */
    static void notifyResourcesCreated();

    /** Called by a resource to notify that loading of events has successfully completed,
     *  or that loading has failed. */
    static void notifyResourcePopulated(const ResourceType*);

    /** Called to notify that a resource is about to be removed. */
    static void notifyResourceToBeRemoved(ResourceType*);

    /** Called by a resource to notify that its settings have changed.
     *  This will cause the settingsChanged() signal to be emitted.
     */
    static void notifySettingsChanged(ResourceType*, ResourceType::Changes, CalEvent::Types oldEnabled);

    /** Called by a resource when a user message should be displayed.
     *  This will cause the resourceMessage() signal to be emitted.
     *  @param message  Must include the resource's display name in order to
     *                  identify the resource to the user.
     */
    static void notifyResourceMessage(ResourceType*, ResourceType::MessageType, const QString& message, const QString& details);

    /** Called when a user message should be displayed for a resource.
     *  This will cause the resourceMessage() signal to be emitted.
     *  @param message  Must include the resource's display name in order to
     *                  identify the resource to the user.
     */
    static void notifyResourceMessage(ResourceId, ResourceType::MessageType, const QString& message, const QString& details);

    /** Called by a resource to notify that it has added events. */
    static void notifyEventsAdded(ResourceType*, const QList<KAEvent>&);

    /** Called by a resource to notify that it has changed an event.
     *  The event's UID must be unchanged.
     */
    static void notifyEventUpdated(ResourceType*, const KAEvent& event);

    /** Called by a resource to notify that it is about to delete events. */
    static void notifyEventsToBeRemoved(ResourceType*, const QList<KAEvent>&);

    /** Called by a resource to notify that it has deleted events. */
    static void notifyEventsRemoved(ResourceType*, const QList<KAEvent>&);

    /** Called by a resource settings instance to notify that it is about to be destructed. */
    static void notifySettingsDestroyed(ResourceId);

Q_SIGNALS:
    /** Emitted when a resource's settings have changed. */
    void settingsChanged(Resource&, ResourceType::Changes);

    /** Emitted when all configured resource have been created (but not
     *  necessarily populated), and any necessary resource migration and
     *  the creation of default resources has been performed.
     */
    void resourcesCreated();

    /** Emitted when all configured and migrated resources have been loaded for
     *  the first time, or cannot currently be loaded.
     *  This is always emitted after resourcesCreated().
     */
    void resourcesPopulated();

    /** Emitted when a new resource has been created.
     *  The resource may or may not have loaded its events before the signal is
     *  emitted.
     */
    void resourceAdded(Resource&);

    /** Emitted when a resource's events have been successfully loaded.
     *
     *  This signal is not emitted if the resource's events have already been
     *  loaded before its resourceAdded() signal is emitted.
     *
     *  @see Resource::isPopulated() can be called at any time to check whether
     *  the resource's events have been loaded.
     */
    void resourcePopulated(Resource&);

    /** Emitted when a resource's config and settings are about to be removed. */
    void resourceToBeRemoved(Resource&);

    /** Emitted when a resource's config and settings have been removed. */
    void resourceRemoved(KAlarmCal::ResourceId);

    /** Emitted when a resource message should be displayed to the user.
     *  @note  Connections to this signal should use Qt::QueuedConnection type
     *         to allow processing to continue while the user message is displayed.
     */
    void resourceMessage(ResourceType::MessageType, const QString& message, const QString& details);

    /** Emitted when events have been added to a resource.
     *  Events are only notified whose alarm type is enabled.
     *
     *  This signal is not emitted when events are added to the resource before
     *  its resourceAdded() signal is emitted.
     */
    void eventsAdded(Resource&, const QList<KAEvent>&);

    /** Emitted when an event has been updated in a resource.
     *  Events are only notified whose alarm type is enabled.
     *  The event's UID is unchanged.
     */
    void eventUpdated(Resource&, const KAEvent&);

    /** Emitted when events are about to be deleted from a resource.
     *  Events are only notified whose alarm type is enabled.
     */
    void eventsToBeRemoved(Resource&, const QList<KAEvent>&);

    /** Emitted when events have been deleted from a resource.
     *  Events are only notified whose alarm type is enabled.
     */
    void eventsRemoved(Resource&, const QList<KAEvent>&);

private:
    Resources();

    /** Add a new ResourceType instance, with a Resource owner.
     *  Once the resource has completed its initialisation, call
     *  notifyNewResourceInitialised() to emit the resourceAdded() signal.
     *  is require
     *  @param type      Newly constructed ResourceType instance, which will belong to
     *                   'resource' if successful. On error, it will be deleted.
     *  @param resource  If type is invalid, updated to an invalid resource;
     *                   If type ID already exists, updated to the existing resource with that ID;
     *                   If type ID doesn't exist, updated to the new resource containing res.
     *  @return true if a new resource has been created, false if invalid or already exists.
     */
    static bool addResource(ResourceType* type, Resource& resource);

    /** Remove the resource with a given ID.
     *  @note  The ResourceType instance will only be deleted once all Resource
     *         instances which refer to this ID go out of scope.
     */
    static void removeResource(ResourceId);

    static void checkResourcesPopulated();

    static Resources*                  mInstance;    // the unique instance
    static QHash<ResourceId, Resource> mResources;   // contains all ResourceType instances with an ID
    static bool                        mCreated;     // all resources have been created
    static bool                        mPopulated;   // all resources have been loaded once

    friend class ResourceType;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(Resources::Sorting)
Q_DECLARE_OPERATORS_FOR_FLAGS(Resources::DestOptions)


/*=============================================================================
* Template definitions.
*============================================================================*/

template <class RType>
QVector<Resource> Resources::allResources(CalEvent::Type type, Sorting sorting)
{
    const CalEvent::Types types = (type == CalEvent::EMPTY)
                                ? CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE
                                : type;

    QVector<Resource> result;
    Resource std;
    if ((sorting & DefaultFirst)  &&  type != CalEvent::EMPTY)
    {
        std = getStandard(type, (type == CalEvent::ARCHIVED));
        if (std.isValid()  &&  std.is<RType>())
            result += std;
    }
    const int start = result.size();

    for (auto it = mResources.constBegin();  it != mResources.constEnd();  ++it)
    {
        const Resource& res = it.value();
        if (res != std  &&  res.is<RType>()  &&  (res.alarmTypes() & types))
            result += res;
    }

    if (sorting & DisplayName)
        std::sort(result.begin() + start, result.end(), [](const Resource& a, const Resource& b) { return a.displayName().compare(b.displayName(), Qt::CaseInsensitive) < 0; });

    return result;
}

// vim: et sw=4:
