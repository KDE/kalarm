/*
 *  compatibilityattribute.h  -  Akonadi attribute holding Collection compatibility
 *  Program:  kalarm
 *  Copyright © 2011 by David Jarvie <djarvie@kde.org>
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
#ifndef KALARM_COMPATIBILITY_ATTRIBUTE_H
#define KALARM_COMPATIBILITY_ATTRIBUTE_H

#include "kalarm_cal_export.h"

#include "kacalendar.h"

#include <akonadi/attribute.h>

namespace KAlarm
{

/**
 * @short An Attribute for a KAlarm Collection containing compatibility information.
 *
 * This class represents an Akonadi attribute of a KAlarm Collection. It contains
 * information on the compatibility of the Collection and its Items with the
 * current KAlarm calendar format. The attribute is maintained by the Akonadi
 * resource, and should be treated as read-only by applications.
 *
 * @see CollectionAttribute
 *
 * @author David Jarvie <djarvie@kde.org>
 */

class KALARM_CAL_EXPORT CompatibilityAttribute : public Akonadi::Attribute
{
    public:
        /** Default constructor. Creates an incompatible attribute. */
        CompatibilityAttribute();

        /** Copy constructor. */
        CompatibilityAttribute(const CompatibilityAttribute& other);

        /** Assignment operator. */
        CompatibilityAttribute& operator=(const CompatibilityAttribute& other);

        virtual ~CompatibilityAttribute();

        /** Return the compatibility status for the entity. */
        KACalendar::Compat compatibility() const;

        /** Set the compatibility status for the entity. */
        void setCompatibility(KACalendar::Compat c);

        /** Return the KAlarm version of the backend calendar format.
         *  @return version number in the format returned by KAlarm::Version().
         */
        int version() const;

        /** Set the KAlarm version of the backend calendar format.
         *  @param v  version number in the format returned by KAlarm::Version().
         */
        void setVersion(int v);

        /** Reimplemented from Attribute */
        virtual QByteArray type() const;
        /** Reimplemented from Attribute */
        virtual CompatibilityAttribute* clone() const;
        /** Reimplemented from Attribute */
        virtual QByteArray serialized() const;
        /** Reimplemented from Attribute */
        virtual void deserialize(const QByteArray& data);

        /** Return the attribute name. */
        static QByteArray name();

    private:
        //@cond PRIVATE
        class Private;
        Private* const d;
        //@endcond
};

} // namespace KAlarm

#endif // KALARM_COMPATIBILITY_ATTRIBUTE_H

// vim: et sw=4:
