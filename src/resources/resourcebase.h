/*
 *  resourcebase.h  -  base class for an alarm calendar resource
 *  Program:  kalarm
 *  Copyright © 2019 David Jarvie <djarvie@kde.org>
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

#ifndef RESOURCEBASE_H
#define RESOURCEBASE_H

#include <kalarmcal/kacalendar.h>
#include <kalarmcal/kaevent.h>

#include <QObject>
#include <QSharedPointer>
#include <QUrl>
#include <QColor>

using namespace KAlarmCal;

/** Abstract base class for an alarm calendar resource. */
class ResourceBase : public QObject
{
    Q_OBJECT
public:
    /** A shared pointer to an Resource object. */
    typedef QSharedPointer<ResourceBase> Ptr;

    ResourceBase()  {}
    virtual ~ResourceBase() = 0;

    /** Return whether the resource has a valid configuration. */
    virtual bool isValid() const = 0;

    /** Return the resource's unique ID. */
    virtual ResourceId id() const = 0;

    /** Return the type of the resource (file, remote file, etc.)
     *  for display purposes.
     *  @param description  true for description (e.g. "Remote file"),
     *                      false for brief label (e.g. "URL").
     */
    virtual QString storageType(bool description) const = 0;

    /** Return the location(s) of the resource (URL, file path, etc.) */
    virtual QUrl location() const = 0;

    /** Return the location of the resource (URL, file path, etc.)
     *  for display purposes. */
    virtual QString displayLocation() const = 0;

    /** Return the resource's display name. */
    virtual QString displayName() const = 0;

    /** Return the resource's configuration identifier. This is not the
     *  name normally displayed to the user.
     */
    virtual QString configName() const = 0;

    /** Return which types of alarms the resource can contain. */
    virtual CalEvent::Types alarmTypes() const = 0;

    /** Return whether the resource is enabled for a specified alarm type
     *  (active, archived, template or displaying).
     *  @param type  alarm type to check for, or EMPTY to check for any type.
     */
    bool isEnabled(CalEvent::Type type) const;

    /** Return which alarm types (active, archived or template) the
     *  resource is enabled for. */
    virtual CalEvent::Types enabledTypes() const = 0;

    /** Set the enabled/disabled state of the resource and its alarms,
     *  for a specified alarm type (active, archived or template). The
     *  enabled/disabled state for other alarm types is not affected.
     *  The alarms of that type in a disabled resource are ignored, and
     *  not displayed in the alarm list. The standard status for that type
     *  for a disabled resource is automatically cleared.
     *  @param type     alarm type
     *  @param enabled  true to set enabled, false to set disabled.
     */
    virtual void setEnabled(CalEvent::Type type, bool enabled) = 0;

    /** Set which alarm types (active, archived or template) the resource
     *  is enabled for.
     *  @param types  alarm types
     */
    virtual void setEnabled(CalEvent::Types types) = 0;

    /** Return whether the resource is configured as read-only or is
     *  read-only on disc.
     */
    virtual bool readOnly() const = 0;

    /** Return whether the resource is both enabled and fully writable for a
     *  given alarm type, i.e. not read-only, and compatible with the current
     *  KAlarm calendar format.
     *
     *  @param type  alarm type to check for, or EMPTY to check for any type.
     *  @return 1 = fully enabled and writable,
     *          0 = enabled and writable except that backend calendar is in an
     *              old KAlarm format,
     *         -1 = read-only, disabled or incompatible format.
     */
    virtual int writableStatus(CalEvent::Type type = CalEvent::EMPTY) const = 0;

    /** Return whether the resource is both enabled and fully writable for a
     *  given alarm type, i.e. not read-only, and compatible with the current
     *  KAlarm calendar format.
     *
     *  @param type  alarm type to check for, or EMPTY to check for any type.
     */
    bool isWritable(CalEvent::Type type = CalEvent::EMPTY) const;

#if 0
    /** Return whether the event can be written to now, i.e. the resource is
     *  active and read-write, and the event is in the current KAlarm format. */
    virtual bool isWritable(const KAEvent&) const = 0;
#endif

    /** Return whether the user has chosen not to update the resource's
     *  calendar storage formst. */
    virtual bool keepFormat() const = 0;

    /** Set or clear whether the user has chosen not to update the resource's
     *  calendar storage formst. */
    virtual void setKeepFormat(bool keep) = 0;

    /** Return the background colour used to display alarms belonging to
     *  this resource.
     *  @return display colour, or invalid if none specified */
    virtual QColor backgroundColour() const = 0;

    /** Set the background colour used to display alarms belonging to this
     *  resource.
     *  @param colour display colour, or invalid to use the default colour */
    virtual void setBackgroundColour(const QColor& colour) = 0;

    /** Return the foreground colour used to display alarms belonging to
     *  this resource, for given alarm type(s).
     *  @param types  Alarm type(s), or EMPTY for the alarm types which the
     *                resource contains.
     *  @return display colour, or invalid if none specified */
    QColor foregroundColour(CalEvent::Types types = CalEvent::EMPTY) const;

    /** Return whether the resource is set in the resource's config to be the
     *  standard resource for a specified alarm type (active, archived or
     *  template). There is no check for whether the resource is enabled, is
     *  writable, or is the only resource set as standard.
     *
     *  @note To determine whether the resource is actually the standard
     *        resource, call the resource manager's isStandard() method.
     *
     *  @param type  alarm type
     */
    virtual bool configIsStandard(CalEvent::Type type) const = 0;

    /** Return which alarm types (active, archived or template) the resource
     *  is standard for, as set in its config. This is restricted to the alarm
     *  types which the resource can contain (@see alarmTypes()).
     *  There is no check for whether the resource is enabled, is writable, or
     *  is the only resource set as standard.
     *
     *  @note To determine what alarm types the resource is actually the standard
     *        resource for, call the resource manager's standardTypes() method.
     *
     *  @return alarm types.
     */
    virtual CalEvent::Types configStandardTypes() const = 0;

    /** Set or clear the resource as the standard resource for a specified alarm
     *  type (active, archived or template), storing the setting in the resource's
     *  config. There is no check for whether the resource is eligible to be
     *  set as standard, or to ensure that it is the only standard resource for
     *  the type.
     *
     *  @note To set the resource's standard status and ensure that it is
     *        eligible and the only standard resource for the type, call
     *        the resource manager's setStandard() method.
     *
     *  @param type      alarm type
     *  @param standard  true to set as standard, false to clear standard status.
     */
    virtual void configSetStandard(CalEvent::Type type, bool standard) = 0;

    /** Set which alarm types (active, archived or template) the resource is
     *  the standard resource for, storing the setting in the resource's config.
     *  There is no check for whether the resource is eligible to be set as
     *  standard, or to ensure that it is the only standard resource for the
     *  types.
     *
     *  @note To set the resource's standard status and ensure that it is
     *        eligible and the only standard resource for the types, call
     *        the resource manager's setStandard() method.
     *
     *  @param types  alarm types.to set as standard
     */
    virtual void configSetStandard(CalEvent::Types types) = 0;

    /** Load the resource from the file, and fetch all events.
     *  If loading is initiated, the ResourceManager will emit the resourceLoaded()
     *  signal on completion.
     *  Loading is not performed if the resource is disabled.
     *  If the resource is cached, it will be loaded from the cache file (which
     *  if @p readThroughCache is true, will first be downloaded from the resource file).
     *
     *  Derived classes must implement loading in doLoad().
     *
     *  @param readThroughCache  If the resource is cached, refresh the cache first.
     *  @return true if loading succeeded or has been initiated.
     *          false if it failed.
     */
    virtual bool load(bool readThroughCache = true) = 0;

#if 0
    /** Reload the resource. Any cached data is first discarded. */
    virtual bool reload() = 0;
#endif

    /** Return whether the resource has fully loaded. */
    virtual bool isLoaded() const = 0;

    /** Save the resource.
     *  If saving is initiated, the ResourceManager will emit the resourceSaved()
     *  signal on completion.
     *  Saving is not performed if the resource is disabled.
     *  If the resource is cached, it will be saved to the cache file (which
     *  if @p writeThroughCache is true, will then be uploaded from the resource file).
     *  @param writeThroughCache  If the resource is cached, update the file
     *                            after writing to the cache.
     *  @return true if saving succeeded or has been initiated.
     *          false if it failed.
     */
    virtual bool save(bool writeThroughCache = true) = 0;

    /** Close the resource, without saving it.
     *  If the resource is currently being saved, it will be closed automatically
     *  once the save has completed.
     *
     *  Derived classes must implement closing in doClose().
     *
     *  @return true if closed, false if waiting for a save to complete.
     */
    virtual bool close() = 0;

    /** Return whether the resource is in a different format from the
     *  current KAlarm format, in which case it cannot be written to.
     *  Note that isWritable() takes account of incompatible format
     *  as well as read-only and enabled statuses.
     */
    virtual KACalendar::Compat compatibility() const = 0;

    /** Return whether the resource is in the current KAlarm format. */
    bool isCompatible() const;

    /** Return all events belonging to this resource.
     *  Derived classes are not guaranteed to implement this.
     */
    virtual QList<KAEvent> events() const   { return {}; }

    /** Called to notify the resource that an event's command error has changed. */
    virtual void handleCommandErrorChange(const KAEvent&) = 0;

protected:
    QString storageTypeString(bool description, bool file, bool local) const;
};

#endif // RESOURCEBASE_H

// vim: et sw=4:
