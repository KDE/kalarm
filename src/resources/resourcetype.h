/*
 *  resourcetype.h  -  base class for an alarm calendar resource type
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2019-2021 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include <KAlarmCal/KACalendar>
#include <KAlarmCal/KAEvent>

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
    /** Flag set in resource ID to distinguish File Resource IDs from Akonadi
     *  Collection IDs.
     *  This is the second-topmost bit, which is extremely unlikely to be set by
     *  Akonadi, and does not make the ID negative.
     */
    static const ResourceId IdFlag = 1LL << (64 - 2);
    static_assert(sizeof(IdFlag) == sizeof(qint64), "ResourceType::IdFlag is wrong");

    /** The type of storage used by a resource. */
    enum StorageType  { NoStorage, File, Directory };

    /** Settings change types. These may be combined.
     *  @note  A resource's location is not allowed to change, except by
     *         deleting the resource and creating another resource with the new
     *         location. (Note that Akonadi resources do actually allow
     *         location change, but this is handled internally by Akonadi and
     *         has no impact on clients.)
     */
    enum Change
    {
        NoChange         = 0,
        Name             = 0x01,   //!< The resource's display name.
        AlarmTypes       = 0x02,   //!< Alarm types contained in the resource.
        Enabled          = 0x04,   //!< Alarm types which are enabled.
        Standard         = 0x08,   //!< Alarm types which the resource is standard for.
        ReadOnly         = 0x10,   //!< The resource's read-only setting.
        KeepFormat       = 0x20,   //!< Whether the user has chosen not to convert to the current KAlarm format.
        UpdateFormat     = 0x40,   //!< The resource should now be converted to the current KAlarm format.
        BackgroundColour = 0x80,   //!< The background colour to display the resource.
        // Non-status types...
        Loaded           = 0x100   //!< The resource has been loaded from file. Note that this is an event notification type, not a status type.
    };
    Q_DECLARE_FLAGS(Changes, Change)

    /** Resource message types. */
    enum class MessageType { Info, Error };

    /** A shared pointer to an Resource object. */
    using Ptr = QSharedPointer<ResourceType>;

    ResourceType()  {}

    /** Constructor.
     *  @param temporary  If false, the new instance will be added to the list
     *                    of instances for lookup;
     *                    If true, it's a temporary instance not added to the list.
     */
    explicit ResourceType(ResourceId);

    virtual ~ResourceType() = 0;

    /** Return whether the resource has a valid configuration.
     *  Note that the resource may be unusable even if it has a valid
     *  configuration: see failed().
     */
    virtual bool isValid() const = 0;

    /** Return whether the resource has a fatal error.
     *  Note that failed() will return true if the configuration is invalid
     *  (i.e. isValid() returns false). It will also return true if some other
     *  error prevents the resource being used, e.g. if the calendar file
     *  cannot be created.
     */
    bool failed() const;

    /** Return the resource's unique ID. */
    ResourceId id() const    { return mId; }

    /** Return the resource's unique ID, as shown to the user. */
    virtual ResourceId displayId() const;

    /** Return the type of storage used by the resource. */
    virtual StorageType storageType() const = 0;

    /** Return the type of the resource (file, remote file, etc.)
     *  for display purposes.
     *  @param description  true for description (e.g. "Remote file"),
     *                      false for brief label (e.g. "URL").
     */
    virtual QString storageTypeString(bool description) const = 0;

    /** Return the type description of a resource (file, remote file, etc.)
     *  for display purposes. This is equivalent to storageTypeString(true).
     */
    static QString storageTypeString(StorageType);

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
     *  calendar storage format. */
    virtual bool keepFormat() const = 0;

    /** Set or clear whether the user has chosen not to update the resource's
     *  calendar storage format. */
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
    virtual KACalendar::Compat compatibilityVersion(QString& versionString) const = 0;

    /** Edit the resource's configuration. */
    virtual void editResource(QWidget* dialogParent) = 0;

    /** Remove the resource. The calendar file is not removed.
     *  Derived classes must call removeResource(ResourceId) once they have removed
     *  the resource, in order to invalidate this instance and remove it from the
     *  list held by Resources.
     *  @note The instance will be invalid once it has been removed.
     *  @return true if the resource has been removed or a removal job has been scheduled.
     */
    virtual bool removeResource() = 0;

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
    virtual bool load(bool readThroughCache = true) = 0;

    /** Reload the resource. Any cached data is first discarded.
     *  @param discardMods  Discard any modifications since the last save.
     *  @return true if loading succeeded or has been initiated.
     *          false if it failed.
     */
    virtual bool reload(bool discardMods = false) = 0;

    /** Return whether the resource has fully loaded.
     *  Once loading completes after the resource has initialised, this should
     *  always return true.
     */
    virtual bool isPopulated() const   { return mLoaded; }

    /** Save the resource.
     *  Saving is not performed if the resource is disabled.
     *  If the resource is cached, it will be saved to the cache file (which
     *  if @p writeThroughCache is true, will then be uploaded from the resource file).
     *  @param errorMessage       If non-null, receives error message if failure,
     *                            in which case no error is displayed by the resource.
     *  @param writeThroughCache  If the resource is cached, update the file
     *                            after writing to the cache.
     *  @param force              Save even if no changes have been made since last
     *                            loaded or saved.
     *  @return true if saving succeeded or has been initiated.
     *          false if it failed.
     */
    virtual bool save(QString* errorMessage = nullptr, bool writeThroughCache = true, bool force = false) = 0;

    /** Return whether the resource is waiting for a save() to complete. */
    virtual bool isSaving() const   { return false; }

    /** Close the resource. This saves any unsaved data.
     *  Saving is not performed if the resource is disabled.
     */
    virtual void close() {}

    /** Return all events belonging to this resource, for enabled alarm types. */
    QList<KAEvent> events() const;

    /** Return the event with the given ID, provided its alarm type is enabled for
     *  the resource.
     *  @param eventId        ID of the event to return.
     *  @param allowDisabled  Return the event even if its alarm type is disabled.
     *  @return  The event, or invalid event if not found or alarm type is disabled.
     */
    KAEvent event(const QString& eventId, bool allowDisabled = false) const;
    using QObject::event;   // prevent "hidden" warning

    /** Return whether the resource contains the event whose ID is given, provided
     *  the event's alarm type is enabled for the resource.
     */
    bool containsEvent(const QString& eventId) const;

    /** Add an event to the resource. */
    virtual bool addEvent(const KAEvent&) = 0;

    /** Update an event in the resource. Its UID must be unchanged.
     *  @param saveIfReadOnly  If the resource is read-only, whether to try to save
     *                         the resource after updating the event. (A writable
     *                         resource will always be saved.)
     */
    virtual bool updateEvent(const KAEvent&, bool saveIfReadOnly = true) = 0;

    /** Delete an event from the resource. */
    virtual bool deleteEvent(const KAEvent&) = 0;

    /** To be called when the start-of-day time has changed, to adjust the start
     *  times of all date-only alarms' recurrences.
     */
    void adjustStartOfDay();

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
    void settingsChanged(KAlarmCal::ResourceId, ResourceType::Changes);

    /** Emitted by the all() instance, when a resource message should be displayed to the user.
     *  @note  Connections to this signal should use Qt::QueuedConnection type.
     *  @param message  Derived classes must include the resource's display name.
     */
    void resourceMessage(KAlarmCal::ResourceId, ResourceType::MessageType, const QString& message, const QString& details);

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

    /** Remove the resource with a given ID.
     *  @note  The ResourceType instance will only be deleted once all Resource
     *         instances which refer to this ID go out of scope.
     */
    static void removeResource(ResourceId);

    /** To be called when the resource has loaded, to update the list of loaded
     *  events for the resource. This should include both enabled and disabled
     *  alarm types.
     */
    void setLoadedEvents(QHash<QString, KAEvent>& newEvents);

    /** To be called when events have been created or updated, to amend them in
     *  the resource's list.
     *  @param notify  Whether to notify added and updated events; if false,
     *                 notifyUpdatedEvents() must be called afterwards.
     */
    void setUpdatedEvents(const QList<KAEvent>& events, bool notify = true);

    /** Notify added and updated events, if setUpdatedEvents() was called with
     *  notify = false.
     */
    void notifyUpdatedEvents();

    /** To be called when events have been deleted, to delete them from the resource's list. */
    void setDeletedEvents(const QList<KAEvent>& events);

    /** To be called when the loaded status of the resource has changed. */
    void setLoaded(bool loaded) const;

    /** To be called if the resource has encountered a fatal error.
     *  A fatal error is one that can never be recovered from.
     */
    void setFailed();

    static QString storageTypeStr(bool description, bool file, bool local);

    template <class T> static T* resource(Resource&);
    template <class T> static const T* resource(const Resource&);

private:
    static ResourceType* data(Resource&);
    static const ResourceType* data(const Resource&);

    QHash<QString, KAEvent> mEvents;     // all events (of ALL types) in the resource, indexed by ID
    QList<KAEvent> mEventsAdded;         // events added to mEvents but not yet notified
    QList<KAEvent> mEventsUpdated;       // events updated in mEvents but not yet notified
    ResourceId   mId {-1};               // resource's ID, which can't be changed
    bool         mFailed {false};        // the resource has a fatal error
    mutable bool mLoaded {false};        // the resource has finished loading
    bool         mBeingDeleted {false};  // the resource is currently being deleted
};

Q_DECLARE_OPERATORS_FOR_FLAGS(ResourceType::Changes)
Q_DECLARE_METATYPE(ResourceType::MessageType)


/*=============================================================================
* Template definitions.
*============================================================================*/

template <class T> T* ResourceType::resource(Resource& res)
{
    return qobject_cast<T*>(data(res));
}

template <class T> const T* ResourceType::resource(const Resource& res)
{
    return qobject_cast<const T*>(data(res));
}


// vim: et sw=4:
