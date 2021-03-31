/*
 *  compatibilityattribute.h  -  Akonadi attribute holding Collection compatibility
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2011-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#pragma once

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

    ~CompatibilityAttribute() override;

    /** Comparison operator. */
    bool operator==(const CompatibilityAttribute &other) const;
    bool operator!=(const CompatibilityAttribute &other) const  { return !operator==(other); }

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
    QByteArray type() const override;
    // Reimplemented from Attribute
    CompatibilityAttribute *clone() const override;
    // Reimplemented from Attribute
    QByteArray serialized() const override;
    // Reimplemented from Attribute
    void deserialize(const QByteArray &data) override;

    /** Return the attribute name. */
    static QByteArray name();

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond
};

} // namespace KAlarmCal


// vim: et sw=4:
