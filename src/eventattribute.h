/*
 *  eventattribute.h  -  per-user attributes for individual events
 *  This file is part of kalarmcal library, which provides access to KAlarm
 *  calendar data.
 *  SPDX-FileCopyrightText: 2010-2011 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

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

    ~EventAttribute() override;

    /** Return the last command execution error for the item. */
    KAEvent::CmdErrType commandError() const;

    /** Set the last command execution error for the item. */
    void setCommandError(KAEvent::CmdErrType err);

    // Reimplemented from Attribute
    QByteArray type() const override;
    // Reimplemented from Attribute
    EventAttribute *clone() const override;
    // Reimplemented from Attribute
    QByteArray serialized() const override;
    // Reimplemented from Attribute
    void deserialize(const QByteArray &data) override;

private:
    //@cond PRIVATE
    class Private;
    Private *const d;
    //@endcond
};

} // namespace KAlarmCal


// vim: et sw=4:
