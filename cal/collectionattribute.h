/*
 *  collectionattribute.h  -  Akonadi attribute holding Collection characteristics
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

#ifndef COLLECTION_ATTRIBUTE_H
#define COLLECTION_ATTRIBUTE_H

#include "kacalendar.h"

#include <akonadi/attribute.h>

#include <QColor>

namespace KAlarm
{

/*=============================================================================
= Class: CollectionAttribute
= User-specific attributes of a KAlarm collection, including the compatibility
= status of collections or items.
=============================================================================*/

class KALARM_CAL_EXPORT CollectionAttribute : public Akonadi::Attribute
{
    public:
        CollectionAttribute()
                : mStandard(KACalEvent::EMPTY),
                  mCompatibility(KACalendar::Incompatible),
                  mEnabled(false) { }

        bool isEnabled() const   { return mEnabled; }

        /** Set the enabled/disabled state of the collection and its alarms.
         *  The alarms in a disabled collection are ignored, and not displayed in the alarm list.
         *  The standard status for a disabled collection is automatically cleared.
         */
        void setEnabled(bool enabled);

        /** Return whether the collection is the standard collection for a specified
         *  mime type. */
        bool isStandard(KACalEvent::Type) const;

        /** Set or clear the collection as the standard collection for a specified
         *  mime type. */
        void setStandard(KACalEvent::Type, bool standard);

        /** Return which mime types the collection is standard for. */
        KACalEvent::Types standard() const     { return mStandard; }

        /** Set which mime types the collection is the standard collection for. */
        void setStandard(KACalEvent::Types t)  { mStandard = t; }

        /** Return the background color to display this collection and its alarms,
         *  or invalid color if none is set.
         */
        QColor backgroundColor() const     { return mBackgroundColour; }

        /** Set the background color for this collection and its alarms. */
        void   setBackgroundColor(const QColor& c)  { mBackgroundColour = c; }

        /** Return the compatibility status for the entity. */
        KACalendar::Compat compatibility() const  { return mCompatibility; }

        /** Set the compatibility status for the entity. */
        void setCompatibility(KACalendar::Compat c)  { mCompatibility = c; }

        virtual QByteArray type() const    { return "KAlarm collection"; }
        virtual CollectionAttribute* clone() const;
        virtual QByteArray serialized() const;
        virtual void deserialize(const QByteArray& data);

    private:
        CollectionAttribute(const CollectionAttribute&);

        QColor              mBackgroundColour; // background color for collection and its alarms
        KACalEvent::Types   mStandard;         // whether the collection is a standard collection
        KACalendar::Compat  mCompatibility;    // calendar compatibility with current KAlarm format
        bool                mEnabled;          // if false, this collection's alarms are ignored
};

} // namespace KAlarm

#endif // COLLECTION_ATTRIBUTE_H

// vim: et sw=4:
