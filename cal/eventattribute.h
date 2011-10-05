/*
 *  eventattribute.h  -  per-user attributes for individual events
 *  Program:  kalarm
 *  Copyright © 2010 by David Jarvie <djarvie@kde.org>
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

#ifndef EVENT_ATTRIBUTE_H
#define EVENT_ATTRIBUTE_H

#include "kalarm_cal_export.h"

#include "kaevent.h"

#include <akonadi/attribute.h>

namespace KAlarm
{

/*=============================================================================
= Class: EventAttribute
= User-specific attributes for an Akonadi item (event).
=============================================================================*/

class KALARM_CAL_EXPORT EventAttribute : public Akonadi::Attribute
{
    public:
        EventAttribute()  : mCommandError(KAEvent::CMD_NO_ERROR) { }
        virtual ~EventAttribute() {}

        /** Return the last command execution error for the item. */
        KAEvent::CmdErrType commandError() const   { return mCommandError; }

        /** Set the last command execution error for the item. */
        void setCommandError(KAEvent::CmdErrType err)  { mCommandError = err; }

        virtual QByteArray type() const    { return "KAlarmEvent"; }
        virtual EventAttribute* clone() const;
        virtual QByteArray serialized() const;
        virtual void deserialize(const QByteArray& data);

    private:
        EventAttribute(const EventAttribute& ea) : Akonadi::Attribute(ea), mCommandError(ea.mCommandError) {}

        KAEvent::CmdErrType mCommandError;         // the last command execution error for the alarm
};

} // namespace KAlarm

#endif // EVENT_ATTRIBUTE_H

// vim: et sw=4:
