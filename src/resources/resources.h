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

#include "resourcetype.h"

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

    static ResourceType* resource(ResourceId);

    static void notifySettingsChanged(ResourceType*, ResourceType::Changes);
    static void notifyResourceMessage(ResourceType*, ResourceType::MessageType, const QString& message, const QString& details);

Q_SIGNALS:
    /** Emitted when the resource's settings have changed. */
    void settingsChanged(ResourceId, ResourceType::Changes);

    /** Emitted when a resource message should be displayed to the user.
     *  @note  Connections to this signal should use Qt::QueuedConnection type.
     *  @param message  Derived classes must include the resource's display name.
     */
    void resourceMessage(ResourceId, ResourceType::MessageType, const QString& message, const QString& details);

private:
    Resources();

    static Resources* mInstance;

    friend class ResourceType;
};

#endif // RESOURCES_H

// vim: et sw=4:
