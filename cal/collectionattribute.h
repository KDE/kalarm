/*
 *  collectionattribute.h  -  Akonadi attribute holding Collection characteristics
 *  Program:  kalarm
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

#ifndef COLLECTION_ATTRIBUTE_H
#define COLLECTION_ATTRIBUTE_H

#include "kalarm_cal_export.h"

#include "kacalendar.h"

#include <akonadi/attribute.h>

#include <QColor>

namespace KAlarm
{

/**
 * @short An Attribute for a KAlarm Collection containing various status information.
 *
 * This class represents an Akonadi attribute of a KAlarm Collection. It contains
 * information on the enabled status, the mime types allowed in the resource,
 * which mime types the resource is the standard Collection for, etc.
 *
 * The attribute is maintained by client applications.
 *
 * @see CompatibilityAttribute
 *
 * @author David Jarvie <djarvie@kde.org>
 */

class KALARM_CAL_EXPORT CollectionAttribute : public Akonadi::Attribute
{
    public:
        CollectionAttribute();

        /** Copy constructor. */
        CollectionAttribute(const CollectionAttribute& other);

        /** Assignment operator. */
        CollectionAttribute& operator=(const CollectionAttribute& other);

        ~CollectionAttribute();

        /** Return whether the collection is enabled for a specified mime type. */
        bool isEnabled(KAlarm::CalEvent::Type type) const;

        /** Return which mime types the collection is enabled for. */
        KAlarm::CalEvent::Types enabled() const;

        /** Set the enabled/disabled state of the collection and its alarms, for a
         *  specified alarm type. The enabled/disabled state for other alarm types
         *  is not affected.
         *  The alarms of that type in a disabled collection are ignored, and not
         *  displayed in the alarm list. The standard status for that type for
         *  a disabled collection is automatically cleared.
         */
        void setEnabled(KAlarm::CalEvent::Type, bool enabled);

        /** Set which mime types the collection enabled for. */
        void setEnabled(KAlarm::CalEvent::Types);

        /** Return whether the collection is the standard collection for a specified
         *  mime type. */
        bool isStandard(KAlarm::CalEvent::Type) const;

        /** Set or clear the collection as the standard collection for a specified
         *  mime type. */
        void setStandard(KAlarm::CalEvent::Type, bool standard);

        /** Return which mime types the collection is standard for. */
        KAlarm::CalEvent::Types standard() const;

        /** Set which mime types the collection is the standard collection for. */
        void setStandard(KAlarm::CalEvent::Types);

        /** Return the background color to display this collection and its alarms,
         *  or invalid color if none is set.
         */
        QColor backgroundColor() const;

        /** Set the background color for this collection and its alarms. */
        void setBackgroundColor(const QColor& c);

        /** Return whether the user has chosen to keep the old calendar storage
         *  format, i.e. not update to current KAlarm format.
         */
        bool keepFormat() const;

        /** Set whether to keep the old calendar storage format unchanged. */
        void setKeepFormat(bool keep);

        /** Reimplemented from Attribute */
        virtual QByteArray type() const;
        /** Reimplemented from Attribute */
        virtual CollectionAttribute* clone() const;
        /** Reimplemented from Attribute */
        virtual QByteArray serialized() const;
        /** Reimplemented from Attribute */
        virtual void deserialize(const QByteArray& data);
        /** Reimplemented from Attribute */
        static QByteArray name();

    private:
        //@cond PRIVATE
        class Private;
        Private* const d;
        //@endcond
};

} // namespace KAlarm

#endif // COLLECTION_ATTRIBUTE_H

// vim: et sw=4:
