/*
 *  compatibilityattribute.h  -  Akonadi attribute holding Collection compatibility
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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

#ifndef COMPATIBILITY_ATTRIBUTE_H
#define COMPATIBILITY_ATTRIBUTE_H

#include "kalarm_cal_export.h"

#include "kacalendar.h"

#include <akonadi/attribute.h>

namespace KAlarm
{

/*=============================================================================
= Class: CompatibilityAttribute
= Resource-specific attributes of a KAlarm collection, i.e. the compatibility
= status of collections or items.
=============================================================================*/

class KALARM_CAL_EXPORT CompatibilityAttribute : public Akonadi::Attribute
{
    public:
        CompatibilityAttribute()
                : mCompatibility(KAlarm::Calendar::Incompatible)  { }

        /** Return the compatibility status for the entity. */
        KAlarm::Calendar::Compat compatibility() const  { return mCompatibility; }

        /** Set the compatibility status for the entity. */
        void setCompatibility(KAlarm::Calendar::Compat c)  { mCompatibility = c; }

        virtual QByteArray type() const    { return name(); }
        virtual CompatibilityAttribute* clone() const;
        virtual QByteArray serialized() const;
        virtual void deserialize(const QByteArray& data);
        static QByteArray name()    { return "KAlarmCompatibility"; }

    private:
        CompatibilityAttribute(const CompatibilityAttribute&);

        KAlarm::Calendar::Compat mCompatibility;    // calendar compatibility with current KAlarm format
};

} // namespace KAlarm

#endif // COMPATIBILITY_ATTRIBUTE_H

// vim: et sw=4:
