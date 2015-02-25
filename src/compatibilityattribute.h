/*
 *  compatibilityattribute.h  -  Akonadi attribute holding Collection compatibility
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
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

#include "kalarmcal_export.h"

#include "kacalendar.h"

#include <attribute.h>

namespace KAlarmCal
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

class KALARMCAL_EXPORT CompatibilityAttribute : public Akonadi::Attribute
{
public:
    /** Default constructor. Creates an incompatible attribute. */
    CompatibilityAttribute();

    /** Copy constructor. */
    CompatibilityAttribute(const CompatibilityAttribute &other);

    /** Assignment operator. */
    CompatibilityAttribute &operator=(const CompatibilityAttribute &other);

    virtual ~CompatibilityAttribute();

    /** Return the compatibility status for the entity. */
    KACalendar::Compat compatibility() const;

    /** Set the compatibility status for the entity. */
    void setCompatibility(KACalendar::Compat c);

    /** Return the KAlarm version of the backend calendar format.
     *  @return version number in the format returned by KAlarmCal::Version().
     */
    int version() const;

    /** Set the KAlarm version of the backend calendar format.
     *  @param v  version number in the format returned by KAlarmCal::Version().
     */
    void setVersion(int v);

    // Reimplemented from Attribute
    QByteArray type() const Q_DECL_OVERRIDE;
    // Reimplemented from Attribute
    CompatibilityAttribute *clone() const Q_DECL_OVERRIDE;
    // Reimplemented from Attribute
    QByteArray serialized() const Q_DECL_OVERRIDE;
    // Reimplemented from Attribute
    void deserialize(const QByteArray &data) Q_DECL_OVERRIDE;

    /** Return the attribute name. */
    static QByteArray name();

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond
};

} // namespace KAlarmCal

#endif // KALARM_COMPATIBILITY_ATTRIBUTE_H

