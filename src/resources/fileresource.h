/*
 *  fileresource.h  -  base class for calendar resource accessed via file system
 *  Program:  kalarm
 *  Copyright © 2020 David Jarvie <djarvie@kde.org>
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

#ifndef FILERESOURCE_H
#define FILERESOURCE_H

#include "resourcetype.h"

#include <AkonadiCore/Collection>
#include <KCalendarCore/FileStorage>

#include <QObject>
#include <QSharedPointer>
#include <QHash>
#include <QList>

class FileResourceSettings;

using namespace KAlarmCal;

/** Base class for an alarm calendar resource accessed directly through the file system.
 *  Public access to this class and derived classes is normally via the Resource class.
 */
class FileResource : public ResourceType
{
    Q_OBJECT
public:
    /** Current status of resource. */
    // IF YOU ALTER THE ORDER OF THIS ENUM, ENSURE THAT ALL VALUES WHICH INDICATE AN
    // UNUSABLE RESOURCE ARE >= 'Unusable'.
    enum class Status
    {
        Ready,          // the resource is ready to use
        Loading,        // the resource is loading, and will be ready soon
        Saving,         // the resource is saving, and will be ready soon
        Broken,         // the resource is in error
        Unusable,       // ... values greater than this indicate an unusable resource
        Closed,         // the resource has been closed. (Closed resources cannot be reopened.)
        NotConfigured   // the resource lacks necessary configuration
    };

    /** Constructor.
     *  Initialises the resource and initiates loading its events.
     */
    explicit FileResource(FileResourceSettings* settings);

    ~FileResource();

    /** Return whether the resource has a valid configuration. */
    bool isValid() const override;

    /** Return the resource's unique ID, as shown to the user. */
    ResourceId displayId() const override;

    /** Return the type of the resource (file, remote file, etc.)
     *  for display purposes.
     *  @param description  true for description (e.g. "Remote file"),
     *                      false for brief label (e.g. "URL").
     */
    QString storageTypeString(bool description) const override;

    /** Return the location(s) of the resource (URL, file path, etc.) */
    QUrl location() const override;

    /** Return the location of the resource (URL, file path, etc.)
     *  for display purposes. */
    QString displayLocation() const override;

    /** Return the resource's display name. */
    QString displayName() const override;

    /** Return the resource's configuration identifier. This is not the
     *  name normally displayed to the user.
     */
    QString configName() const override;

    /** Return which types of alarms the resource can contain. */
    CalEvent::Types alarmTypes() const override;

    /** Return which alarm types (active, archived or template) the
     *  resource is enabled for. */
    CalEvent::Types enabledTypes() const override;

    /** Set the enabled/disabled state of the resource and its alarms,
     *  for a specified alarm type (active, archived or template). The
     *  enabled/disabled state for other alarm types is not affected.
     *  The alarms of that type in a disabled resource are ignored, and
     *  not displayed in the alarm list. The standard status for that type
     *  for a disabled resource is automatically cleared.
     *  @param type     alarm type
     *  @param enabled  true to set enabled, false to set disabled.
     */
    void setEnabled(CalEvent::Type type, bool enabled) override;

    /** Set which alarm types (active, archived or template) the resource
     *  is enabled for.
     *  @param types  alarm types
     */
    void setEnabled(CalEvent::Types types) override;

    /** Return whether the resource is configured as read-only or is
     *  read-only on disc.
     */
    bool readOnly() const override;

    /** Specify the read-only configuration status of the resource. */
    void setReadOnly(bool);

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
    int writableStatus(CalEvent::Type type = CalEvent::EMPTY) const override;

    using ResourceType::isWritable;

    /** Return whether the event can be written to now, i.e. the resource is
     *  active and read-write, and the event is in the current KAlarm format. */
    bool isWritable(const KAEvent&) const;

    /** Return whether the user has chosen not to update the resource's
     *  calendar storage format. */
    bool keepFormat() const override;

    /** Set or clear whether the user has chosen not to update the resource's
     *  calendar storage format. */
    void setKeepFormat(bool keep) override;

    /** Return the background colour used to display alarms belonging to
     *  this resource.
     *  @return display colour, or invalid if none specified */
    QColor backgroundColour() const override;

    /** Set the background colour used to display alarms belonging to this
     *  resource.
     *  @param colour display colour, or invalid to use the default colour */
    void setBackgroundColour(const QColor& colour) override;

    /** Return whether the resource is set in the resource's config to be the
     *  standard resource for a specified alarm type (active, archived or
     *  template). There is no check for whether the resource is enabled, is
     *  writable, or is the only resource set as standard.
     *
     *  @note To determine whether the resource is actually the standard
     *        resource, call FileResourceManager::isStandard().
     *
     *  @param type  alarm type
     */
    bool configIsStandard(CalEvent::Type type) const override;

    /** Return which alarm types (active, archived or template) the resource
     *  is standard for, as set in its config. This is restricted to the alarm
     *  types which the resource can contain (@see alarmTypes()).
     *  There is no check for whether the resource is enabled, is writable, or
     *  is the only resource set as standard.
     *
     *  @note To determine what alarm types the resource is actually the standard
     *        resource for, call FileResourceManager::standardTypes().
     *
     *  @return alarm types.
     */
    CalEvent::Types configStandardTypes() const override;

    /** Set or clear the resource as the standard resource for a specified alarm
     *  type (active, archived or template), storing the setting in the resource's
     *  config. There is no check for whether the resource is eligible to be
     *  set as standard, or to ensure that it is the only standard resource for
     *  the type.
     *
     *  @note To set the resource's standard status and ensure that it is
     *        eligible and the only standard resource for the type, call
     *        FileResourceManager::setStandard().
     *
     *  @param type      alarm type
     *  @param standard  true to set as standard, false to clear standard status.
     */
    void configSetStandard(CalEvent::Type type, bool standard) override;

    /** Set which alarm types (active, archived or template) the resource is
     *  the standard resource for, storing the setting in the resource's config.
     *  There is no check for whether the resource is eligible to be set as
     *  standard, or to ensure that it is the only standard resource for the
     *  types.
     *
     *  @note To set the resource's standard status and ensure that it is
     *        eligible and the only standard resource for the types, call
     *        FileResourceManager::setStandard().
     *
     *  @param types  alarm types.to set as standard
     */
    void configSetStandard(CalEvent::Types types) override;

    /** Return whether the resource is in a different format from the
     *  current KAlarm format, in which case it cannot be written to.
     *  Note that isWritable() takes account of incompatible format
     *  as well as read-only and enabled statuses.
     *  @param versionString  Receives calendar's KAlarm version as a string.
     */
    KACalendar::Compat compatibilityVersion(QString& versionString) const override;

    /** Edit the resource's configuration. */
    void editResource(QWidget* dialogParent) override;

    /** Remove the resource. The calendar file is not removed.
     *  @note The instance will be invalid once it has been removed.
     *  @return true if the resource has been removed or a removal job has been scheduled.
     */
    bool removeResource() override;

    /** Return the current status of the resource. */
    Status status() const    { return mStatus; }

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
    bool load(bool readThroughCache = true) override;

    /** Save the resource.
     *  Saving is not performed if the resource is disabled.
     *  If the resource is cached, it will be saved to the cache file (which
     *  if @p writeThroughCache is true, will then be uploaded from the resource file).
     *  @param writeThroughCache  If the resource is cached, update the file
     *                            after writing to the cache.
     *  @param force              Save even if no changes have been made since last
     *                            loaded or saved.
     *  @return true if saving succeeded or has been initiated.
     *          false if it failed.
     */
    bool save(bool writeThroughCache = true, bool force = false) override;

    /** Add an event to the resource.
     *  Derived classes must implement event addition in doAddEvent().
     */
    bool addEvent(const KAEvent&) override;

    /** Update an event in the resource. Its UID must be unchanged.
     *  Derived classes must implement event update in doUpdateEvent().
     */
    bool updateEvent(const KAEvent&) override;

    /** Delete an event from the resource.
     *  Derived classes must implement event deletion in doDeleteEvent().
     */
    bool deleteEvent(const KAEvent&) override;

    /** Called to notify the resource that an event's command error has changed. */
    void handleCommandErrorChange(const KAEvent&) override;

virtual void showProgress(bool)  {}

    /*-----------------------------------------------------------------------------
    * The methods below are all particular to the FileResource class, and in order
    * to be accessible to clients are defined as 'static'.
    *----------------------------------------------------------------------------*/

    /** Update a resource to the current KAlarm storage format. */
    static bool updateStorageFormat(Resource&);

protected:
    /** Identifier for use in cache file names etc. */
    QString identifier() const;

    /** Find the compatibility of an existing calendar file. */
    static KACalendar::Compat getCompatibility(const KCalendarCore::FileStorage::Ptr& fileStorage, int& version);

    /** Update the resource to the current KAlarm storage format. */
    virtual bool updateStorageFmt() = 0;

    /** This method is called by load() to allow derived classes to implement
     *  loading the resource from its backend, and fetch all events into
     *  @p newEvents.
     *  If the resource is cached, it should be loaded from the cache file (which
     *  if @p readThroughCache is true, should first be downloaded from the
     *  resource file).
     *  If the resource initiates but does not complete loading, loaded() must be
     *  called when loading completes or fails.
     *  @see loaded()
     *
     *  @param newEvents         To be updated to contain the events fetched.
     *  @param readThroughCache  If the resource is cached, refresh the cache first.
     *  @return 1 = loading succeeded
     *          0 = loading has been initiated, but has not yet completed
     *         -1 = loading failed.
     */
    virtual int doLoad(QHash<QString, KAEvent>& newEvents, bool readThroughCache, QString& errorMessage) = 0;

    /** To be called by derived classes on completion of loading the resource,
     *  only if doLoad() initiated but did not complete loading.
     *  @param success    true if loading succeeded, false if failed.
     *  @param newEvents  The events which have been fetched.
     */
    void loaded(bool success, QHash<QString, KAEvent>& newEvents, const QString& errorMessage);

    /** Schedule the resource for saving.
     *  Derived classes may reimplement this method to delay calling save(), so
     *  as to enable multiple event changes to be saved together.
     *  The default is to call save() immediately.
     *
     *  @return true if saving succeeded or has been initiated/scheduled.
     *          false if it failed.
     */
    virtual bool scheduleSave(bool writeThroughCache = true)  { return save(writeThroughCache); }

    /** This method is called by save() to allow derived classes to implement
     *  saving the resource to its backend.
     *  If the resource is cached, it should be saved to the cache file (which
     *  if @p writeThroughCache is true, should then be uploaded from the
     *  resource file).
     *  If the resource initiates but does not complete saving, saved() must be
     *  called when saving completes or fails.
     *  @see saved()
     *
     *  @param writeThroughCache  If the resource is cached, update the file
     *                            after writing to the cache.
     *  @param force              Save even if no changes have been made since last
     *                            loaded or saved.
     *  @return 1 = saving succeeded
     *          0 = saving has been initiated, but has not yet completed
     *         -1 = saving failed.
     */
    virtual int doSave(bool writeThroughCache, bool force, QString& errorMessage) = 0;

    /** Determine whether the resource can be saved. If not, an error message
     *  will be displayed to the user.
     */
    bool checkSave();

    /** To be called by derived classes on completion of saving the resource,
     *  only if doSave() initiated but did not complete saving.
     *  @param success    true if saving succeeded, false if failed.
     */
    void saved(bool success, const QString& errorMessage);

    /** This method is called by addEvent() to allow derived classes to add
     *  an event to the resource.
     */
    virtual bool doAddEvent(const KAEvent&) = 0;

    /** This method is called by updateEvent() to allow derived classes to update
     *  an event in the resource. The event's UID must be unchanged.
     */
    virtual bool doUpdateEvent(const KAEvent&) = 0;

    /** This method is called by deleteEvent() to allow derived classes to delete
     *  an event from the resource.
     */
    virtual bool doDeleteEvent(const KAEvent&) = 0;

    /** Called when settings have changed, to allow derived classes to process
     *  the changes.
     *  @note  Resources::notifySettingsChanged() is called after this, to
     *         notify clients.
     */
    virtual void handleSettingsChange(Changes)  {}

    FileResourceSettings* mSettings;    // the resource's configuration
    int                   mVersion {KACalendar::IncompatibleFormat}; // the calendar format version
    KACalendar::Compat    mCompatibility {KACalendar::Incompatible}; // whether resource is in compatible format

/*
typedef QHash<const QString&, KACalendar::Compat>  CompatibilityMap;  // indexed by event ID
CompatibilityMap    mCompatibilityMap;  // whether individual events are in compatible format
*/
    Status                mStatus {Status::NotConfigured};  // current status of resource
};

#endif // FILERESOURCE_H

// vim: et sw=4:
