/*
 *  eventattribute.h  -  per-user attributes for individual events
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  Copyright © 2010-2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Library General Public License as published
 *  by the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301, USA.
 */

#ifndef KALARM_EVENT_ATTRIBUTE_H
#define KALARM_EVENT_ATTRIBUTE_H

#include "kalarmcal_export.h"

#include "kaevent.h"

#include <attribute.h>

namespace KAlarmCal
{

/**
 * @short An Attribute containing status information for a KAlarm item.
 *
 * This class represents an Akonadi attribute of a KAlarm Item. It contains
 * information on the command execution error status of the event
 * represented by the Item.
 *
 * The attribute is maintained by client applications.
 *
 * @author David Jarvie <djarvie@kde.org>
 */

class KALARMCAL_EXPORT EventAttribute : public Akonadi::Attribute
{
public:
    EventAttribute();

    /** Copy constructor. */
    EventAttribute(const EventAttribute &other);

    /** Assignment operator. */
    EventAttribute &operator=(const EventAttribute &other);

    virtual ~EventAttribute();

    /** Return the last command execution error for the item. */
    KAEvent::CmdErrType commandError() const;

    /** Set the last command execution error for the item. */
    void setCommandError(KAEvent::CmdErrType err);

    // Reimplemented from Attribute
    virtual QByteArray type() const;
    // Reimplemented from Attribute
    virtual EventAttribute *clone() const;
    // Reimplemented from Attribute
    virtual QByteArray serialized() const;
    // Reimplemented from Attribute
    virtual void deserialize(const QByteArray &data);

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond
};

} // namespace KAlarmCal

#endif // KALARM_EVENT_ATTRIBUTE_H

