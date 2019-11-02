/*
 *  resource.h  -  generic class containing an alarm calendar resource
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

#ifndef RESOURCE_H
#define RESOURCE_H

#include "resourcetype.h"

using namespace KAlarmCal;

/** Class for an alarm calendar resource.
 *  It contains a shared pointer to an alarm calendar resource inherited from
 *  ResourceType.
 *  This class is designed to be safe to call even if the pointer to the resource
 *  is null.
 */
class Resource
{
public:
    /** The type of storage used by a resource. */
    enum StorageType
    {
        NoStorage  = ResourceType::NoStorage,
        File       = ResourceType::File,
        Directory  = ResourceType::Directory
    };

    Resource();
    explicit Resource(ResourceType*);
    Resource(const Resource&) = default;
    ~Resource();

    bool operator==(const Resource&) const;
    bool operator==(const ResourceType*) const;

    /** Return a null resource. */
    static Resource null();

    /** Return whether the resource has a null calendar resource pointer. */
    bool isNull() const;

    /** Return whether the resource has a valid configuration. */
    bool isValid() const;

    /** Return the resource's unique ID. */
    ResourceId id() const;

    /** Return the type of storage used by the resource. */
    StorageType storageType() const;

    /** Return the type of the resource (file, remote file, etc.)
     *  for display purposes.
     *  @param description  true for description (e.g. "Remote file"),
     *                      false for brief label (e.g. "URL").
     */
    QString storageTypeString(bool description) const;

    /** Return the location(s) of the resource (URL, file path, etc.) */
    QUrl location() const;

    /** Return the location of the resource (URL, file path, etc.)
     *  for display purposes. */
    QString displayLocation() const;

    /** Return the resource's display name. */
    QString displayName() const;

    /** Return the resource's configuration identifier. This is not the
     *  name normally displayed to the user.
     */
    QString configName() const;

    /** Return which types of alarms the resource can contain. */
    CalEvent::Types alarmTypes() const;

    /** Return whether the resource is enabled for a specified alarm type
     *  (active, archived, template or displaying).
     *  @param type  alarm type to check for, or EMPTY to check for any type.
     */
    bool isEnabled(CalEvent::Type type) const;

    /** Return which alarm types (active, archived or template) the
     *  resource is enabled for. */
    CalEvent::Types enabledTypes() const;

    /** Set the enabled/disabled state of the resource and its alarms,
     *  for a specified alarm type (active, archived or template). The
     *  enabled/disabled state for other alarm types is not affected.
     *  The alarms of that type in a disabled resource are ignored, and
     *  not displayed in the alarm list. The standard status for that type
     *  for a disabled resource is automatically cleared.
     *  @param type     alarm type
     *  @param enabled  true to set enabled, false to set disabled.
     */
    void setEnabled(CalEvent::Type type, bool enabled);

    /** Set which alarm types (active, archived or template) the resource
     *  is enabled for.
     *  @param types  alarm types
     */
    void setEnabled(CalEvent::Types types);

    /** Return whether the resource is configured as read-only or is
     *  read-only on disc.
     */
    bool readOnly() const;

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
    int writableStatus(CalEvent::Type type = CalEvent::EMPTY) const;

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
    bool isWritable(const KAEvent&) const;
#endif

    /** Return whether the user has chosen not to update the resource's
     *  calendar storage formst. */
    bool keepFormat() const;

    /** Set or clear whether the user has chosen not to update the resource's
     *  calendar storage formst. */
    void setKeepFormat(bool keep);

    /** Return the background colour used to display alarms belonging to
     *  this resource.
     *  @return display colour, or invalid if none specified */
    QColor backgroundColour() const;

    /** Set the background colour used to display alarms belonging to this
     *  resource.
     *  @param colour display colour, or invalid to use the default colour */
    void setBackgroundColour(const QColor& colour);

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
    bool configIsStandard(CalEvent::Type type) const;

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
    CalEvent::Types configStandardTypes() const;

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
    void configSetStandard(CalEvent::Type type, bool standard);

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
    void configSetStandard(CalEvent::Types types);

    /** Return whether the resource is in a different format from the
     *  current KAlarm format, in which case it cannot be written to.
     *  Note that isWritable() takes account of incompatible format
     *  as well as read-only and enabled statuses.
     */
    KACalendar::Compat compatibility() const;

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
    bool load(bool readThroughCache = true);

#if 0
    /** Reload the resource. Any cached data is first discarded. */
    bool reload();
#endif

    /** Return whether the resource has fully loaded. */
    bool isLoaded() const;

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
    bool save(bool writeThroughCache = true);

    /** Close the resource, without saving it.
     *  If the resource is currently being saved, it will be closed automatically
     *  once the save has completed.
     *
     *  Derived classes must implement closing in doClose().
     *
     *  @return true if closed, false if waiting for a save to complete.
     */
    bool close();

    /** Return all events belonging to this resource.
     *  Derived classes are not guaranteed to implement this.
     */
    QList<KAEvent> events() const;

    /** Add an event to the resource. */
    bool addEvent(const KAEvent&);

    /** Update an event in the resource. Its UID must be unchanged. */
    bool updateEvent(const KAEvent&);

    /** Delete an event from the resource. */
    bool deleteEvent(const KAEvent&);

    /** Called to notify the resource that an event's command error has changed. */
    void handleCommandErrorChange(const KAEvent&);

    /** Must be called to notify the resource that it is being deleted.
     *  This is to prevent expected errors being displayed to the user.
     *  @see isBeingDeleted
     */
    void notifyDeletion();

    /** Return whether the resource has been notified that it is being deleted.
     *  @see notifyDeletion
     */
    bool isBeingDeleted() const;

private:
    /** Return the shared pointer to the alarm calendar resource for this resource.
     *  @warning  The instance referred to by the pointer will be deleted when all
     *            Resource instances containing it go out of scope or are deleted,
     *            so do not pass the pointer to another function.
     */
    template <class T> T* resource() const;

    ResourceType::Ptr mResource;

    friend class ResourceType;   // needs access to resource()
    friend uint qHash(const Resource& resource, uint seed);
};

inline bool operator==(const ResourceType* a, const Resource& b)  { return b == a; }
inline bool operator!=(const Resource& a, const Resource& b)      { return !(a == b); }
inline bool operator!=(const Resource& a, const ResourceType* b)  { return !(a == b); }
inline bool operator!=(const ResourceType* a, const Resource& b)  { return !(b == a); }

inline uint qHash(const Resource& resource, uint seed)
{
    return qHash(resource.mResource.data(), seed);
}

/*****************************************************************************/

template <class T> T* Resource::resource() const
{
    return qobject_cast<T*>(mResource.data());
}

#endif // RESOURCE_H

// vim: et sw=4:
