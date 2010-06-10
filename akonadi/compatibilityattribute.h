/*
 *  compatibilityattribute.h  -  attribute holding calendar format compatibility
 *  Program:  kalarm
 *  Copyright © 2010 by David Jarvie <djarvie@kde.org>
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

#include "kacalendar.h"

#include <akonadi/attribute.h>


/*=============================================================================
= Class: CompatibilityAttribute
= Compatibility status of collections or items.
=============================================================================*/

class CompatibilityAttribute : public Akonadi::Attribute
{
    public:
        CompatibilityAttribute()  : mCompatibility(KACalendar::Incompatible) {}
        virtual ~CompatibilityAttribute() {}

        /** Return the compatibility status for the entity. */
        KACalendar::Compat compatibility() const  { return mCompatibility; }

        /** Set the compatibility status for the entity. */
        void setCompatibility(KACalendar::Compat c)  { mCompatibility = c; }

        virtual QByteArray type() const    { return "compatibility"; }
        virtual CompatibilityAttribute* clone() const;
        virtual QByteArray serialized() const;
        virtual void deserialize(const QByteArray& data);

    private:
        CompatibilityAttribute(const CompatibilityAttribute& a) : Akonadi::Attribute(a), mCompatibility(a.mCompatibility) {}

        KACalendar::Compat mCompatibility;
};

#endif // COMPATIBILITY_ATTRIBUTE_H

// vim: et sw=4:
