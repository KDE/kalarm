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

Q_SIGNALS:
    /** Emitted when a resource's settings have changed. */
    void settingsChanged(Resource&, ResourceType::Changes);

    /** Emitted when a resource message should be displayed to the user.
     *  @note  Connections to this signal should use Qt::QueuedConnection type
     *         to allow processing to continue while the user message is displayed.
     */
    void resourceMessage(Resource&, ResourceType::MessageType, const QString& message, const QString& details);

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

    static Resources*                  mInstance;    // the unique instance
    static QHash<ResourceId, Resource> mResources;   // contains all ResourceType instances with an ID

    friend class ResourceType;
};

#endif // RESOURCES_H

// vim: et sw=4:
