/*
 *  resourcetype.h  -  base class for an alarm calendar resource type
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

#ifndef RESOURCETYPE_H
#define RESOURCETYPE_H

#include <kalarmcal/kacalendar.h>
#include <kalarmcal/kaevent.h>

#include <QObject>
#include <QSharedPointer>
#include <QUrl>
#include <QColor>

class Resource;

using namespace KAlarmCal;

/** Abstract base class for an alarm calendar resource type. */
class ResourceType : public QObject
{
    Q_OBJECT
public:
    /** The type of storage used by a resource. */
    enum StorageType  { NoStorage, File, Directory };

    /** Settings change types. */
    enum Change
    {
        NoChange         = 0,
        Name             = 0x01,
        AlarmTypes       = 0x02,
        Enabled          = 0x04,
        Standard         = 0x08,
        ReadOnly         = 0x10,
        KeepFormat       = 0x20,
        UpdateFormat     = 0x40,
        BackgroundColour = 0x80
    };
    Q_DECLARE_FLAGS(Changes, Change)

    /** Resource message types. */
    enum class MessageType { Info, Error };

    /** A shared pointer to an Resource object. */
    typedef QSharedPointer<ResourceType> Ptr;

    ResourceType()  {}

    /** Constructor.
     *  @param temporary  If false, the new instance will be added to the list
     *                    of instances for lookup;
     *                    If true, it's a temporary instance not added to the list.
     */
    explicit ResourceType(ResourceId);

    virtual ~ResourceType() = 0;

    /** Return whether the resource has a valid configuration. */
    virtual bool isValid() const = 0;

    /** Return the resource's unique ID. */
    ResourceId id() const    { return mId; }

    /** Return the type of storage used by the resource. */
    virtual StorageType storageType() const = 0;

    /** Return the type of the resource (file, remote file, etc.)
     *  for display purposes.
     *  @param description  true for description (e.g. "Remote file"),
     *                      false for brief label (e.g. "URL").
     */
    virtual QString storageTypeString(bool description) const = 0;

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

    /** Return whether the resource is in a different format from the
     *  current KAlarm format, in which case it cannot be written to.
     *  Note that isWritable() takes account of incompatible format
     *  as well as read-only and enabled statuses.
     */
    virtual KACalendar::Compat compatibility() const = 0;

    /** Return whether the resource is in the current KAlarm format. */
    bool isCompatible() const;

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
    virtual bool isLoaded() const   { return mLoaded; }

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

    /** Return all events belonging to this resource.
     *  Derived classes are not guaranteed to implement this.
     */
    virtual QList<KAEvent> events() const   { return {}; }

    /** Add an event to the resource. */
    virtual bool addEvent(const KAEvent&) = 0;

    /** Update an event in the resource. Its UID must be unchanged. */
    virtual bool updateEvent(const KAEvent&) = 0;

    /** Delete an event from the resource. */
    virtual bool deleteEvent(const KAEvent&) = 0;

    /** Called to notify the resource that an event's command error has changed. */
    virtual void handleCommandErrorChange(const KAEvent&) = 0;

    /** Must be called to notify the resource that it is being deleted.
     *  This is to prevent expected errors being displayed to the user.
     *  @see isBeingDeleted
     */
    void notifyDeletion();

    /** Return whether the resource has been notified that it is being deleted.
     *  @see notifyDeletion
     */
    bool isBeingDeleted() const;

Q_SIGNALS:
    /** Emitted by the all() instance, when the resource's settings have changed. */
    void settingsChanged(ResourceId, Changes);

    /** Emitted by the all() instance, when a resource message should be displayed to the user.
     *  @note  Connections to this signal should use Qt::QueuedConnection type.
     *  @param message  Derived classes must include the resource's display name.
     */
    void resourceMessage(ResourceId, MessageType, const QString& message, const QString& details);

protected:
    /** Add a new ResourceType instance, with a Resource owner.
     *  @param type      Newly constructed ResourceType instance, which will belong to
     *                   'resource' if successful. On error, it will be deleted.
     *  @param resource  If type is invalid, updated to an invalid resource;
     *                   If type ID already exists, updated to the existing resource with that ID;
     *                   If type ID doesn't exist, updated to the new resource containing res.
     *  @return true if a new resource has been created, false if invalid or already exists.
     */
    static bool addResource(ResourceType* type, Resource& resource);

    void setLoaded(bool loaded) const;
    QString storageTypeStr(bool description, bool file, bool local) const;
    template <class T> static T* resource(Resource&);
    template <class T> static const T* resource(const Resource&);

private:
    static ResourceType* data(Resource&);
    static const ResourceType* data(const Resource&);

    ResourceId   mId{-1};               // resource's ID, which can't be changed
    mutable bool mLoaded{false};        // the resource has finished loading
    bool         mBeingDeleted{false};  // the resource is currently being deleted
};

/*****************************************************************************/

template <class T> T* ResourceType::resource(Resource& res)
{
    return qobject_cast<T*>(data(res));
}

template <class T> const T* ResourceType::resource(const Resource& res)
{
    return qobject_cast<const T*>(data(res));
}

#endif // RESOURCETYPE_H

// vim: et sw=4:
