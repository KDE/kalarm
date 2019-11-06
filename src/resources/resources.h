/*
 *  resources.h  -  container for all ResourceType instances
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

#ifndef RESOURCES_H
#define RESOURCES_H

#include "resource.h"

#include <QObject>
class QEventLoop;

using namespace KAlarmCal;

/** Class to contain all ResourceType instances.
 *  It provides connection to signals from all ResourceType instances.
 */
class Resources : public QObject
{
    Q_OBJECT
public:
    /** Creates the unique Resources instance. */
    static Resources* instance();

    ~Resources() {}
    Resources(const Resources&) = delete;
    Resources& operator=(const Resources&) const = delete;

    /** Return a copy of the resource with a given ID.
     *  @return  The resource, or invalid if the ID doesn't already exist or is invalid.
     */
    static Resource resource(ResourceId);

    /** Remove the resource with a given ID.
     *  @note  The ResourceType instance will only be deleted once all Resource
     *         instances which refer to this ID go out of scope.
     */
    static void removeResource(ResourceId);

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
     *  @param type  alarm type
     *  @return standard resource, or null if none.
     */
    static Resource getStandard(CalEvent::Type type);

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

    /** Return whether all configured resources have been created. */
    static bool allCreated();

    /** Return whether all configured resources have been loaded at least once. */
    static bool allPopulated();

    /** Wait until one or all enabled resources have been populated,
     *  i.e. whether their events have been fetched.
     *  @param   resId    resource ID, or -1 for all resources
     *  @param   timeout  timeout in seconds, or 0 for no timeout
     *  @return  true if successful.
     */
    static bool waitUntilPopulated(ResourceId resId = -1, int timeout = 0);

    /** Return the resource which an event belongs to, provided that the event's
     *  alarm type is enabled. */
    static Resource resourceForEvent(const QString& eventId);

    /** Return the resource which an event belongs to, and the event, provided
     *  that the event's alarm type is enabled. */
    static Resource resourceForEvent(const QString& eventId, KAEvent& event);

    /** Called to notify that all configured resources have now been created. */
    static void notifyResourcesCreated();

    /** Called by a resource to notify that loading has successfully completed. */
    static void notifyResourceLoaded(const ResourceType*);

    /** Called by a resource to notify that its settings have changed.
     *  This will cause the settingsChanged() signal to be emitted.
     */
    static void notifySettingsChanged(ResourceType*, ResourceType::Changes);

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

    /** Called by a resource to notify that it has changed an event. */
    static void notifyEventUpdated(ResourceType*, const KAEvent& event);

    /** Called by a resource to notify that it is about to delete events. */
    static void notifyEventsToBeRemoved(ResourceType*, const QList<KAEvent>&);

Q_SIGNALS:
    /** Emitted when a resource's settings have changed. */
    void settingsChanged(Resource&, ResourceType::Changes);

    /** Emitted when all configured resource have been created (but not
     *  necessarily populated). */
    void resourcesCreated();

    /** Emitted when all configured resources have been loaded for the first time. */
    void resourcesPopulated();

    /** Emitted when a resource's events have been successfully loaded. */
    void resourceLoaded(Resource&);

    /** Emitted when a resource's config and settings have been removed. */
    void resourceRemoved(ResourceId);

    /** Emitted when a resource message should be displayed to the user.
     *  @note  Connections to this signal should use Qt::QueuedConnection type
     *         to allow processing to continue while the user message is displayed.
     */
    void resourceMessage(Resource&, ResourceType::MessageType, const QString& message, const QString& details);

    /** Emitted when events have been added to a resource.
     *  Events are only notified whose alarm type is enabled.
     */
    void eventsAdded(Resource&, const QList<KAEvent>&);

    /** Emitted when an event has been updated in a resource.
     *  Events are only notified whose alarm type is enabled.
     */
    void eventUpdated(Resource&, const KAEvent&);

    /** Emitted when events are about to be deleted from a resource.
     *  Events are only notified whose alarm type is enabled.
     */
    void eventsToBeRemoved(Resource&, const QList<KAEvent>&);

private:
    Resources();

    /** Add a new ResourceType instance, with a Resource owner.
     *  @param type      Newly constructed ResourceType instance, which will belong to
     *                   'resource' if successful. On error, it will be deleted.
     *  @param resource  If type is invalid, updated to an invalid resource;
     *                   If type ID already exists, updated to the existing resource with that ID;
     *                   If type ID doesn't exist, updated to the new resource containing res.
     *  @return true if a new resource has been created, false if invalid or already exists.
     */
    static bool addResource(ResourceType* type, Resource& resource);
    static void checkResourcesPopulated();
    static bool isLoaded(ResourceId);

    static Resources*                  mInstance;    // the unique instance
    static QHash<ResourceId, Resource> mResources;   // contains all ResourceType instances with an ID
    static QEventLoop*                 mPopulatedCheckLoop;
    static bool                        mCreated;     // all resources have been created
    static bool                        mPopulated;   // all resources have been loaded once

    friend class ResourceType;
};

#endif // RESOURCES_H

// vim: et sw=4: