/*
 *  resource.h  -  generic class containing an alarm calendar resource
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

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
    enum class Storage
    {
        None       = static_cast<int>(ResourceType::Storage::None),
        File       = static_cast<int>(ResourceType::Storage::File),
        Directory  = static_cast<int>(ResourceType::Storage::Directory)
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

    /** Return whether the resource has a valid configuration.
     *  Note that the resource may be unusable even if it has a valid
     *  configuration: see failed().
     */
    bool isValid() const;

    /** Return whether the resource has a fatal error.
     *  Note that failed() will return true if the configuration is invalid
     *  (i.e. isValid() returns false). It will also return true if some other
     *  error prevents the resource being used, e.g. if the calendar file
     *  cannot be created.
     */
    bool failed() const;

    /** Return whether the resource has an error (fatal or non-fatal), and
     *  cannot currently be used. This will be true if failed() is true.
     */
    bool inError() const;

    /** Return the resource's unique ID. */
    ResourceId id() const;

    /** Return the resource's unique ID, as shown to the user. */
    ResourceId displayId() const;

    /** Return the type of storage used by the resource. */
    Storage storageType() const;

    /** Return the type of the resource (file, remote file, etc.)
     *  for display purposes.
     *  @param description  true for description (e.g. "Remote file"),
     *                      false for brief label (e.g. "URL").
     */
    QString storageTypeString(bool description) const;

    /** Return the type description of a resource (file, remote file, etc.)
     *  for display purposes. This is equivalent to storageTypeString(true).
     */
    static QString storageTypeString(ResourceType::Storage);

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
     *  calendar storage format. */
    bool keepFormat() const;

    /** Set or clear whether the user has chosen not to update the resource's
     *  calendar storage format. */
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

    /** Return whether the resource is in the current KAlarm format.
     *  @see compatibility(), compatibilityVersion()
     */
    bool isCompatible() const;

    /** Return whether the resource is in a different format from the
     *  current KAlarm format, in which case it cannot be written to.
     *  Note that isWritable() takes account of incompatible format
     *  as well as read-only and enabled statuses.
     */
    KACalendar::Compat compatibility() const;

    /** Return whether the resource is in a different format from the
     *  current KAlarm format, in which case it cannot be written to.
     *  Note that isWritable() takes account of incompatible format
     *  as well as read-only and enabled statuses.
     *  @param versionString  Receives calendar's KAlarm version as a string.
     */
    KACalendar::Compat compatibilityVersion(QString& versionString) const;

    /** Edit the resource's configuration. */
    void editResource(QWidget* dialogParent);

    /** Remove the resource. The calendar file is not removed.
     *  @return true if the resource has been removed or a removal job has been scheduled.
     *  @note The instance will be invalid once it has been removed.
     */
    bool removeResource();

    /** Load the resource from the file, and fetch all events.
     *  If loading is initiated, Resources::resourcePopulated() will be emitted
     *  on completion.
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

    /** Reload the resource. Any cached data is first discarded.
     *  @param discardMods  Discard any modifications since the last save.
     *  @return true if loading succeeded or has been initiated.
     *          false if it failed.
     */
    bool reload(bool discardMods = false);

    /** Return whether the resource has fully loaded. */
    bool isPopulated() const;

    /** Save the resource.
     *  Saving is not performed if the resource is disabled.
     *  If the resource is cached, it will be saved to the cache file (which
     *  if @p writeThroughCache is true, will then be uploaded from the resource file).
     *  @param errorMessage       If non-null, receives error message if failure,
     *                            in which case no error is displayed by the resource.
     *  @param writeThroughCache  If the resource is cached, update the file
     *                            after writing to the cache.
     *  @return true if saving succeeded or has been initiated.
     *          false if it failed.
     */
    bool save(QString* errorMessage = nullptr, bool writeThroughCache = true);

    /** Return whether the resource is waiting for a save() to complete. */
    bool isSaving() const;

    /** Close the resource. This saves any unsaved data.
     *  Saving is not performed if the resource is disabled.
     */
    void close();

    /** Return all events belonging to this resource, for enabled alarm types. */
    QList<KAEvent> events() const;

    /** Return the event with the given ID, provided its alarm type is enabled for
     *  the resource.
     *  @param eventId        ID of the event to return.
     *  @param allowDisabled  Return the event even if its alarm type is disabled.
     *  @return  The event, or invalid event if not found or alarm type is disabled.
     */
    KAEvent event(const QString& eventId, bool allowDisabled = false) const;

    /** Return whether the resource contains the event whose ID is given, provided
     *  the event's alarm type is enabled for the resource.
     */
    bool containsEvent(const QString& eventId) const;

    /** Add an event to the resource. */
    bool addEvent(const KAEvent&);

    /** Update an event in the resource. Its UID must be unchanged.
     *  @param saveIfReadOnly  If the resource is read-only, whether to try to save
     *                         the resource after updating the event. (A writable
     *                         resource will always be saved.)
     */
    bool updateEvent(const KAEvent&, bool saveIfReadOnly = true);

    /** Delete an event from the resource. */
    bool deleteEvent(const KAEvent&);

    /** To be called when the start-of-day time has changed, to adjust the start
     *  times of all date-only alarms' recurrences.
     */
    void adjustStartOfDay();

    /** Called to notify the resource that an event's command error has changed. */
    void handleCommandErrorChange(const KAEvent&);

    /** Called when the resource's settings object is about to be destroyed. */
    void removeSettings();

    /** Must be called to notify the resource that it is being deleted.
     *  This is to prevent expected errors being displayed to the user.
     *  @see isBeingDeleted
     */
    void notifyDeletion();

    /** Return whether the resource has been notified that it is being deleted.
     *  @see notifyDeletion
     */
    bool isBeingDeleted() const;

    /** Check whether the resource is of a specified type.
     *  @tparam Type  The resource type to check.
     */
    template <class Type> bool is() const;

private:
    /** Return the shared pointer to the alarm calendar resource for this resource.
     *  @warning  The instance referred to by the pointer will be deleted when all
     *            Resource instances containing it go out of scope or are deleted,
     *            so do not pass the pointer to another function.
     */
    template <class T> T* resource() const;

    ResourceType::Ptr mResource;

    friend class ResourceType;   // needs access to resource()
    friend size_t qHash(const Resource& resource, size_t seed);
};

inline bool operator==(const ResourceType* a, const Resource& b)  { return b == a; }
inline bool operator!=(const Resource& a, const Resource& b)      { return !(a == b); }
inline bool operator!=(const Resource& a, const ResourceType* b)  { return !(a == b); }
inline bool operator!=(const ResourceType* a, const Resource& b)  { return !(b == a); }

inline size_t qHash(const Resource& resource, size_t seed)
{
    return qHash(resource.mResource.data(), seed);
}


/*=============================================================================
* Template definitions.
*============================================================================*/

template <class Type> bool Resource::is() const
{
    return static_cast<bool>(qobject_cast<Type*>(mResource.data()));
}

template <class T> T* Resource::resource() const
{
    return qobject_cast<T*>(mResource.data());
}

// vim: et sw=4:
