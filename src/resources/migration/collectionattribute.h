/*
 *  collectionattribute.h  -  Akonadi attribute holding Collection characteristics
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2010-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#pragma once

#include "kalarmcalendar/kacalendar.h"

#include <Akonadi/Attribute>

#include <QColor>

/**
 * @short An Attribute for a KAlarm Collection containing various status information.
 *
 * This class represents an Akonadi attribute of a legacy KAlarm Collection. It
 * contains information on the enabled status, the alarm types allowed in the
 * resource, which alarm types the resource is the standard Collection for, etc.
 *
 * This class is only used for migrating from legacy KAlarm Akonadi collections.
 *
 * @see CompatibilityAttribute
 *
 * @author David Jarvie <djarvie@kde.org>
 */

class CollectionAttribute : public Akonadi::Attribute
{
public:
    CollectionAttribute();

    /** Copy constructor. */
    CollectionAttribute(const CollectionAttribute& other);

    /** Assignment operator. */
    CollectionAttribute& operator=(const CollectionAttribute& other);

    ~CollectionAttribute() override;

    /** Comparison operator. */
    bool operator==(const CollectionAttribute& other) const;
    bool operator!=(const CollectionAttribute& other) const  { return !operator==(other); }

    /** Return whether the collection is enabled for a specified alarm type
     *  (active, archived, template or displaying).
     *  @param type  alarm type to check for.
     */
    bool isEnabled(KAlarmCal::CalEvent::Type type) const;

    /** Return which alarm types (active, archived or template) the
     *  collection is enabled for. */
    KAlarmCal::CalEvent::Types enabled() const;

    /** Set the enabled/disabled state of the collection and its alarms,
     *  for a specified alarm type (active, archived or template). The
     *  enabled/disabled state for other alarm types is not affected.
     *  The alarms of that type in a disabled collection are ignored, and
     *  not displayed in the alarm list. The standard status for that type
     *  for a disabled collection is automatically cleared.
     *  @param type     alarm type
     *  @param enabled  true to set enabled, false to set disabled.
     */
    void setEnabled(KAlarmCal::CalEvent::Type type, bool enabled);

    /** Set which alarm types (active, archived or template) the collection
     *  is enabled for.
     *  @param types  alarm types
     */
    void setEnabled(KAlarmCal::CalEvent::Types types);

    /** Return whether the collection is the standard collection for a specified
     *  alarm type (active, archived or template).
     *  @param type  alarm type
     */
    bool isStandard(KAlarmCal::CalEvent::Type type) const;

    /** Set or clear the collection as the standard collection for a specified
     *  alarm type (active, archived or template).
     *  @param type      alarm type
     *  @param standard  true to set as standard, false to clear standard status.
     */
    void setStandard(KAlarmCal::CalEvent::Type, bool standard);

    /** Return which alarm types (active, archived or template) the
     *  collection is standard for.
     *  @return alarm types.
     */
    KAlarmCal::CalEvent::Types standard() const;

    /** Set which alarm types (active, archived or template) the
     *  collection is the standard collection for.
     *  @param types  alarm types.
     */
    void setStandard(KAlarmCal::CalEvent::Types types);

    /** Return the background color to display this collection and its alarms,
     *  or invalid color if none is set.
     */
    QColor backgroundColor() const;

    /** Set the background color for this collection and its alarms.
     *  @param c  background color
     */
    void setBackgroundColor(const QColor& c);

    /** Return whether the user has chosen to keep the old calendar storage
     *  format, i.e. not update to current KAlarm format.
     */
    bool keepFormat() const;

    /** Set whether to keep the old calendar storage format unchanged.
     *  @param keep  true to keep format unchanged, false to allow changes.
     */
    void setKeepFormat(bool keep);

    // Reimplemented from Attribute
    QByteArray type() const override;
    // Reimplemented from Attribute
    CollectionAttribute* clone() const override;
    // Reimplemented from Attribute
    QByteArray serialized() const override;
    // Reimplemented from Attribute
    void deserialize(const QByteArray& data) override;

    /** Return the attribute name. */
    static QByteArray name();

private:
    //@cond PRIVATE
    class Private;
    Private* const d;
    //@endcond
};

// vim: et sw=4:
