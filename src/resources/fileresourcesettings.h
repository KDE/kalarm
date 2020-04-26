/*
 *  fileresourcesettings.h  -  settings for calendar resource accessed via file system
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

#ifndef FILERESOURCESETTINGS_H
#define FILERESOURCESETTINGS_H

#include "fileresource.h"

#include <KAlarmCal/KACalendar>
#include <KAlarmCal/KAEvent>

#include <QUrl>
#include <QColor>
#include <QByteArray>
#include <QSharedPointer>
#include <QHash>

class KConfig;
class KConfigGroup;

using namespace KAlarmCal;

class FileResourceSettings
{
public:
    /** A shared pointer to a FileResourceSettings object. */
    typedef QSharedPointer<FileResourceSettings> Ptr;

    enum StorageType { NoStorage, File, Directory };

    struct CommandError
    {
        QString             id;
        KAEvent::CmdErrType error;
    };

    FileResourceSettings()  {}

    /** Initialises the settings from a config file section. */
    FileResourceSettings(KConfig* config, const QString& resourceGroup);

    /** Initialises the settings. This should only be used by the
     *  ResourceMigrator.
     */
    FileResourceSettings(StorageType, const QUrl& location, CalEvent::Types alarmTypes,
                         const QString& displayName, const QColor& backgroundColour,
                         CalEvent::Types enabledTypes, CalEvent::Types standardTypes,
                         bool readOnly);

    ~FileResourceSettings();

    /** Reads the settings from the resource's config file section.
     *  The settings are validated and amended to be consistent.
     *  @return true if the settings are valid, false if not.
     */
    bool readConfig();

    /** Sets the settings into a new config file section for the resource, and
     *  write the config to disc.
     *  The settings will not be set if they are invalid.
     *  The settings are validated and amended to be consistent.
     *  @return true if the settings were written, false if invalid.
     */
    bool createConfig(KConfig* config, const QString& resourceGroup);

    /** Writes the settings to the config file. */
    void save() const;

    /** Return whether the settings contain valid data. */
    bool isValid() const;

    /** Return the resource's unique ID. */
    ResourceId id() const;

    /** Return the resource's storage location, as a URL. */
    QUrl url() const;

    /** Return the resource's storage location, as a displayable string. */
    QString displayLocation() const;

    /** Return the resource's storage type. */
    StorageType storageType() const;

    /** Return the resource's display name. */
    QString displayName() const;

    /** Set the resource's display name.
     *  @param name  display name to set
     *  @param save  whether to save the config
     */
    ResourceType::Changes setDisplayName(const QString& name, bool save = true);

    /** Return the resource's configuration identifier. This is not the
     *  name normally displayed to the user.
     */
    QString configName() const;

    /** Return which alarm types (active, archived or template) the resource
     *  can contain. */
    CalEvent::Types alarmTypes() const;

    /** Set which alarm types (active, archived or template) the resource can
     *  contain.
     *  @param types  alarm types
     *  @param save   whether to save the config
     */
    ResourceType::Changes setAlarmTypes(CalEvent::Types types, bool save = true);

    /** Return whether the resource is enabled for a specified alarm type
     *  (active, archived, template or displaying).
     *  @param type  alarm type to check for.
     */
    bool isEnabled(CalEvent::Type type) const;

    /** Return which alarm types (active, archived or template) the resource
     *  is enabled for. This is restricted to the alarm types which the
     *  resource can contain (@see alarmTypes()).
     *  @return alarm types.
     */
    CalEvent::Types enabledTypes() const;

    /** Set the enabled/disabled state of the resource and its alarms,
     *  for a specified alarm type (active, archived or template). The
     *  enabled/disabled state for other alarm types is not affected.
     *  The alarms of that type in a disabled resource are ignored, and
     *  not displayed in the alarm list. The standard status for that type
     *  for a disabled resource is automatically cleared.
     *  @param type     alarm type
     *  @param enabled  true to set enabled, false to set disabled.
     *  @param save     whether to save the config
     */
    ResourceType::Changes setEnabled(CalEvent::Type type, bool enabled, bool save = true);

    /** Set which alarm types (active, archived or template) the resource
     *  is enabled for.
     *  @param types  alarm types to enable
     *  @param save   whether to save the config
     */
    ResourceType::Changes setEnabled(CalEvent::Types types, bool save = true);

    /** Return whether the resource is the standard resource for a specified
     *  alarm type (active, archived or template). There is no check for
     *  whether the resource is enabled or whether it is writable.
     *  @param type  alarm type
     */
    bool isStandard(CalEvent::Type type) const;

    /** Return which alarm types (active, archived or template) the resource
     *  is standard for. This is restricted to the alarm types which the
     *  resource can contain (@see alarmTypes()). There is no check for
     *  whether the resource is enabled or whether it is writable.
     *  @return alarm types.
     */
    CalEvent::Types standardTypes() const;

    /** Set or clear the resource as the standard resource for a specified
     *  alarm type (active, archived or template).
     *  If an alarm type is newly set as standard, that alarm type will be
     *  cleared as standard in all other settings instances.
     *  @param type      alarm type
     *  @param standard  true to set as standard, false to clear standard status.
     *  @param save      whether to save the config
     */
    ResourceType::Changes setStandard(CalEvent::Type, bool standard, bool save = true);

    /** Set which alarm types (active, archived or template) the
     *  resource is the standard resource for.
     *  If an alarm type is newly set as standard, that alarm type will be
     *  cleared as standard in all other settings instances.
     *  @param types  alarm types.to set as standard
     *  @param save   whether to save the config
     */
    ResourceType::Changes setStandard(CalEvent::Types types, bool save = true);

    /** Return the background colour to display this resource and its alarms,
     *  or invalid colour if none is set.
     */
    QColor backgroundColour() const;

    /** Set the background colour for this resource and its alarms.
     *  @param c     background colour
     *  @param save  whether to save the config
     */
    ResourceType::Changes setBackgroundColour(const QColor& c, bool save = true);

    /** Return whether the resource is specified as read-only. */
    bool readOnly() const;

    /** Specify the read-only status of the resource.
     *  Note that even if set NOT read-only, it will not be writable if it
     *  is not in the current KAlarm calendar format.
     *  @param ronly  true to set read-only, false to allow writes.
     *  @param save   whether to save the config
     */
    ResourceType::Changes setReadOnly(bool ronly, bool save = true);

    /** Return whether the user has chosen to keep the old calendar storage
     *  format, i.e. not update to current KAlarm format.
     */
    bool keepFormat() const;

    /** Set whether to keep the old calendar storage format unchanged.
     *  @param keep  true to keep format unchanged, false to allow changes.
     *  @param save  whether to save the config
     */
    ResourceType::Changes setKeepFormat(bool keep, bool save = true);

    /** Return whether the user has chosen to update the calendar storage
     *  format to the current KAlarm format.
     */
    bool updateFormat() const;

    /** Set whether to update the calendar storage format to the current
     *  KAlarm format.
     *  @param update  true to update format, false to leave unchanged
     *  @param save    whether to save the config
     */
    ResourceType::Changes setUpdateFormat(bool update, bool save = true);

    /** Return the saved hash of the calendar file contents. */
    QByteArray hash() const;

    /** Set the saved hash of the calendar file contents.
     *  @param hash  hash value
     *  @param save  whether to save the config
     */
    void setHash(const QByteArray& hash, bool save = true);

    /** Return the command error data for all events in the resource which have
     *  command errors.
     *  @return command error types, indexed by event ID.
     */
    QHash<QString, KAEvent::CmdErrType> commandErrors() const;

    /** Set the command error data for all events in the resource which have
     *  command errors.
     *  @param cmdErrors  command error types, indexed by event ID.
     *  @param save       whether to save the config
     */
    void setCommandErrors(const QHash<QString, KAEvent::CmdErrType>& cmdErrors, bool save = true);

protected:
    /** Set the resource's unique ID.
     *  This method can only be called when initialising the instance.
     */
    void setId(ResourceId id);

    friend class FileResourceConfigManager;

private:
    bool                  validate();
    CalEvent::Types       readAlarmTypes(const char* key) const;
    static QString        alarmTypesString(CalEvent::Types alarmTypes);
    static StorageType    storageType(const QUrl& url);
    ResourceType::Changes handleEnabledChange(CalEvent::Types oldEnabled, CalEvent::Types oldStandard, bool typesChanged, bool save);
    ResourceType::Changes handleStandardChange(CalEvent::Types oldStandard, bool sync);
    void writeConfigDisplayName(bool save);
    void writeConfigAlarmTypes(bool save);
    void writeConfigEnabled(bool save);
    void writeConfigStandard(bool save);
    void writeConfigBackgroundColour(bool save);
    void writeConfigReadOnly(bool save);
    void writeConfigKeepFormat(bool save);
    void writeConfigUpdateFormat(bool save);
    void writeConfigHash(bool save);
    void writeConfigCommandErrors(bool save);

    KConfigGroup*     mConfigGroup {nullptr}; // the config group holding all this resource's config
                                              // Until this is set, no notifications will be made
    ResourceId        mId {-1};          // resource's unique ID
    QUrl              mUrl;              // location of file or directory
    QString           mDisplayLocation;  // displayable location of file or directory
    QString           mDisplayName;      // name for user display
    QByteArray        mHash;             // hash of the calendar file contents
    QHash<QString, KAEvent::CmdErrType> mCommandErrors;  // event IDs and their command error types
    QColor            mBackgroundColour; // background colour to display the resource and its alarms
    StorageType       mStorageType {NoStorage};      // how the calendar is stored
    CalEvent::Types   mAlarmTypes {CalEvent::EMPTY}; // alarm types which the resource contains
    CalEvent::Types   mEnabled {CalEvent::EMPTY};    // alarm types for which the resource is enabled
    CalEvent::Types   mStandard {CalEvent::EMPTY};   // alarm types for which the resource is the standard resource
    bool              mReadOnly {false};  // the resource is read-only
    bool              mKeepFormat {true}; // do not update the calendar file to the current KAlarm format
    bool              mUpdateFormat {false}; // request to update the calendar file to the current KAlarm format
};

#endif // FILERESOURCESETTINGS_H

// vim: et sw=4:
