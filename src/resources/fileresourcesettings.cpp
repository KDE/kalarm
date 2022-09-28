/*
 *  fileresourcesettings.cpp  -  settings for calendar resource accessed via file system
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fileresourcesettings.h"

#include <KConfig>
#include <KConfigGroup>

#include <QFileInfo>

namespace
{
// Config file keys
const char* KEY_ID           = "Id";
const char* KEY_TYPE         = "Type";
const char* KEY_PATH         = "Path";
const char* KEY_NAME         = "Name";
const char* KEY_COLOUR       = "Colour";
const char* KEY_ALARMTYPES   = "AlarmTypes";
const char* KEY_ENABLED      = "Enabled";
const char* KEY_STANDARD     = "Standard";
const char* KEY_READONLY     = "ReadOnly";
const char* KEY_KEEPFORMAT   = "KeepFormat";
const char* KEY_UPDATEFORMAT = "UpdateFormat";
const char* KEY_HASH         = "Hash";
const char* KEY_CMDERRORS    = "CommandErrors";
// Config file values
const QLatin1String STORAGE_FILE("File");
const QLatin1String STORAGE_DIR("Dir");
const QLatin1String ALARM_ACTIVE("Active");
const QLatin1String ALARM_ARCHIVED("Archived");
const QLatin1String ALARM_TEMPLATE("Template");
const QLatin1String CMD_ERROR_VALUE("Main");
const QLatin1String CMD_ERROR_PRE_VALUE("Pre");
const QLatin1String CMD_ERROR_POST_VALUE("Post");
const QLatin1String CMD_ERROR_PRE_POST_VALUE("PrePost");
const QLatin1Char   CMD_ERROR_SEPARATOR(':');
}

/******************************************************************************
* Constructor, to initialise from the config.
*/
FileResourceSettings::FileResourceSettings(KConfig* config, const QString& resourceGroup)
    : mConfigGroup(new KConfigGroup(config, resourceGroup))
{
    readConfig();
}

/******************************************************************************
* Constructor, to initialise with manual settings.
*/
FileResourceSettings::FileResourceSettings(StorageType storageType,
                                                 const QUrl& location, CalEvent::Types alarmTypes,
                                                 const QString& displayName, const QColor& backgroundColour,
                                                 CalEvent::Types enabledTypes, CalEvent::Types standardTypes,
                                                 bool readOnly)
    : mId(-1)
    , mUrl(location)
    , mDisplayName(displayName)
    , mBackgroundColour(backgroundColour)
    , mStorageType(storageType)
    , mAlarmTypes(alarmTypes)
    , mEnabled(enabledTypes)
    , mStandard(standardTypes)
    , mReadOnly(readOnly)
{
    validate();   // validate and amend settings to be consistent
}

FileResourceSettings::~FileResourceSettings()
{
    delete mConfigGroup;
}

/******************************************************************************
* Update the instance from the config file.
*/
bool FileResourceSettings::readConfig()
{
    mId                = mConfigGroup->readEntry(KEY_ID, -1);
    if (mId >= 0)
        mId |= ResourceType::IdFlag;   // IDs are saved with IdFlag stripped out
    const QString path = mConfigGroup->readPathEntry(KEY_PATH, QString());
    mUrl               = path.isEmpty() ? QUrl() : QUrl::fromUserInput(path);
    mDisplayLocation   = mUrl.toDisplayString(QUrl::PrettyDecoded | QUrl::PreferLocalFile);
    mDisplayName       = mConfigGroup->readEntry(KEY_NAME, QString());
    mBackgroundColour  = mConfigGroup->readEntry(KEY_COLOUR, QColor());
    mReadOnly          = mConfigGroup->readEntry(KEY_READONLY, false);
    mKeepFormat        = mConfigGroup->readEntry(KEY_KEEPFORMAT, false);
    mUpdateFormat      = mConfigGroup->readEntry(KEY_UPDATEFORMAT, false);
    mHash              = QByteArray::fromHex(mConfigGroup->readEntry(KEY_HASH, QByteArray()));
    mAlarmTypes        = readAlarmTypes(KEY_ALARMTYPES);
    mEnabled           = readAlarmTypes(KEY_ENABLED);
    mStandard          = readAlarmTypes(KEY_STANDARD);

    // Read storage type and validate against location.
    const QString storageType = mConfigGroup->readEntry(KEY_TYPE, QString());
    if (storageType == STORAGE_FILE)
        mStorageType = File;
    else if (storageType == STORAGE_DIR)
        mStorageType = Directory;
    else
    {
        mStorageType = NoStorage;
        return false;
    }
    if (!validate())   // validate and amend settings to be consistent
        return false;

    if (!(mAlarmTypes & CalEvent::ACTIVE))
    {
        // The resource doesn't contain active alarms, so remove any command
        // error information.
        if (mConfigGroup->hasKey(KEY_CMDERRORS))
        {
            mConfigGroup->deleteEntry(KEY_CMDERRORS);
            mConfigGroup->sync();
        }
    }
    else
    {
        // Read command error information.
        mCommandErrors.clear();
        const QStringList cmdErrs = mConfigGroup->readEntry(KEY_CMDERRORS, QStringList());
        for (const QString& cmdErr : cmdErrs)
        {
            int i = cmdErr.indexOf(CMD_ERROR_SEPARATOR);
            if (i > 0  &&  i < cmdErr.size() - 1)
            {
                KAEvent::CmdErrType type;
                const QString typeStr = cmdErr.mid(i + 1);
                if (typeStr == CMD_ERROR_VALUE)
                    type = KAEvent::CMD_ERROR;
                else if (typeStr == CMD_ERROR_PRE_VALUE)
                    type = KAEvent::CMD_ERROR_PRE;
                else if (typeStr == CMD_ERROR_POST_VALUE)
                    type = KAEvent::CMD_ERROR_POST;
                else if (typeStr == CMD_ERROR_PRE_POST_VALUE)
                    type = KAEvent::CMD_ERROR_PRE_POST;
                else
                    continue;
                mCommandErrors[cmdErr.left(i)] = type;
            }
        }
    }

    return true;
}

/******************************************************************************
* Create a config group and write all settings to the config file.
*/
bool FileResourceSettings::createConfig(KConfig* config, const QString& resourceGroup)
{
    delete mConfigGroup;
    mConfigGroup = new KConfigGroup(config, resourceGroup);

    if (!isValid())
        return false;
    if (!validate())   // validate and amend settings to be consistent
        return false;

    QString storage;
    if (mStorageType == File)
        storage = STORAGE_FILE;
    else if (mStorageType == Directory)
        storage = STORAGE_DIR;

    // Save the ID, but strip out IdFlag to make it more legible.
    mConfigGroup->writeEntry(KEY_ID, mId & ~ResourceType::IdFlag);
    mConfigGroup->writePathEntry(KEY_PATH, mDisplayLocation);
    mConfigGroup->writeEntry(KEY_TYPE, storage);
    writeConfigDisplayName(false);
    writeConfigAlarmTypes(false);
    writeConfigEnabled(false);
    writeConfigStandard(false);
    writeConfigBackgroundColour(false);
    writeConfigReadOnly(false);
    writeConfigKeepFormat(false);
    writeConfigUpdateFormat(false);
    writeConfigHash(false);
    writeConfigCommandErrors(false);
    mConfigGroup->sync();
    return true;
}

void FileResourceSettings::save() const
{
    if (mConfigGroup)
        mConfigGroup->sync();
}

bool FileResourceSettings::isValid() const
{
    return (mId >= 0) && (mStorageType != NoStorage) && mUrl.isValid();
}

ResourceId FileResourceSettings::id() const
{
    return mId;
}

void FileResourceSettings::setId(ResourceId id)
{
    if (id != mId)
        mId = id;
}

QUrl FileResourceSettings::url() const
{
    return mUrl;
}

QString FileResourceSettings::displayLocation() const
{
    return mDisplayLocation;
}

FileResourceSettings::StorageType FileResourceSettings::storageType() const
{
    return mStorageType;
}

QString FileResourceSettings::displayName() const
{
    return mDisplayName;
}

ResourceType::Changes FileResourceSettings::setDisplayName(const QString& name, bool sync)
{
    if (name == mDisplayName)
        return ResourceType::NoChange;

    mDisplayName = name;
    if (mConfigGroup)
        writeConfigDisplayName(sync);
    return ResourceType::Name;
}

QString FileResourceSettings::configName() const
{
    return mConfigGroup->name();
}

CalEvent::Types FileResourceSettings::alarmTypes() const
{
    return mAlarmTypes;
}

ResourceType::Changes FileResourceSettings::setAlarmTypes(CalEvent::Types types, bool sync)
{
    if (types == mAlarmTypes)
        return ResourceType::NoChange;

    const CalEvent::Types oldEnabled  = mEnabled;
    const CalEvent::Types oldStandard = mStandard;

    mAlarmTypes = types;
    mEnabled  &= types;
    mStandard &= types;

    return handleEnabledChange(oldEnabled, oldStandard, true, sync);
}

bool FileResourceSettings::isEnabled(CalEvent::Type type) const
{
    return static_cast<bool>(mEnabled & mAlarmTypes & type);
}

CalEvent::Types FileResourceSettings::enabledTypes() const
{
    return mEnabled & mAlarmTypes;
}

ResourceType::Changes FileResourceSettings::setEnabled(CalEvent::Type type, bool enabled, bool sync)
{
    switch (type)
    {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
            break;
        default:
            return ResourceType::NoChange;
    }

    const CalEvent::Types oldEnabled  = mEnabled;
    const CalEvent::Types oldStandard = mStandard;

    if (enabled)
        mEnabled |= type;
    else
    {
        mEnabled  &= ~type;
        mStandard &= ~type;
    }

    return handleEnabledChange(oldEnabled, oldStandard, false, sync);
}

ResourceType::Changes FileResourceSettings::setEnabled(CalEvent::Types types, bool sync)
{
    const CalEvent::Types oldEnabled  = mEnabled;
    const CalEvent::Types oldStandard = mStandard;

    mEnabled  = types & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    mStandard &= mEnabled;

    return handleEnabledChange(oldEnabled, oldStandard, false, sync);
}

ResourceType::Changes FileResourceSettings::handleEnabledChange(CalEvent::Types oldEnabled, CalEvent::Types oldStandard, bool typesChanged, bool sync)
{
    ResourceType::Changes changes = ResourceType::NoChange;
    if (typesChanged)
    {
        if (mConfigGroup)
            writeConfigAlarmTypes(false);
        changes |= ResourceType::AlarmTypes;
    }
    if (mEnabled != oldEnabled)
    {
        if (mConfigGroup)
            writeConfigEnabled(false);
        changes |= ResourceType::Enabled;
    }
    changes |= handleStandardChange(oldStandard, false);
    if (mConfigGroup && sync)
        mConfigGroup->sync();
    return changes;
}

bool FileResourceSettings::isStandard(CalEvent::Type type) const
{
    if (mAlarmTypes & type)
    {
        switch (type)
        {
            case CalEvent::ACTIVE:
            case CalEvent::ARCHIVED:
            case CalEvent::TEMPLATE:
                return static_cast<bool>(mStandard & type);
            default:
                break;
        }
    }
    return false;
}

CalEvent::Types FileResourceSettings::standardTypes() const
{
    return mStandard & mAlarmTypes;
}

ResourceType::Changes FileResourceSettings::setStandard(CalEvent::Type type, bool standard, bool sync)
{
    switch (type)
    {
        case CalEvent::ACTIVE:
        case CalEvent::ARCHIVED:
        case CalEvent::TEMPLATE:
            break;
        default:
            return ResourceType::NoChange;
    }

    const CalEvent::Types oldStandard = mStandard;
    mStandard = standard ? static_cast<CalEvent::Types>(mStandard | type)
                         : static_cast<CalEvent::Types>(mStandard & ~type);
    return handleStandardChange(oldStandard, sync);
}

ResourceType::Changes FileResourceSettings::setStandard(CalEvent::Types types, bool sync)
{
    const CalEvent::Types oldStandard = mStandard;
    mStandard = types & (CalEvent::ACTIVE | CalEvent::ARCHIVED | CalEvent::TEMPLATE);
    return handleStandardChange(oldStandard, sync);
}

ResourceType::Changes FileResourceSettings::handleStandardChange(CalEvent::Types oldStandard, bool sync)
{
    if (mStandard == oldStandard)
        return ResourceType::NoChange;

    if (mConfigGroup)
        writeConfigStandard(sync);
    return ResourceType::Standard;
}

QColor FileResourceSettings::backgroundColour() const
{
    return mBackgroundColour;
}

ResourceType::Changes FileResourceSettings::setBackgroundColour(const QColor& c, bool sync)
{
    if (c == mBackgroundColour)
        return ResourceType::NoChange;

    mBackgroundColour = c;
    if (mConfigGroup)
        writeConfigBackgroundColour(sync);
    return ResourceType::BackgroundColour;
}

bool FileResourceSettings::readOnly() const
{
    return mReadOnly;
}

ResourceType::Changes FileResourceSettings::setReadOnly(bool ronly, bool sync)
{
    if (ronly == mReadOnly)
        return ResourceType::NoChange;

    mReadOnly = ronly;
    if (mConfigGroup)
        writeConfigReadOnly(sync);
    return ResourceType::ReadOnly;
}

bool FileResourceSettings::keepFormat() const
{
    return mKeepFormat;
}

ResourceType::Changes FileResourceSettings::setKeepFormat(bool keep, bool sync)
{
    if (keep == mKeepFormat)
        return ResourceType::NoChange;

    mKeepFormat = keep;
    if (mConfigGroup)
        writeConfigKeepFormat(sync);
    return ResourceType::KeepFormat;
}

bool FileResourceSettings::updateFormat() const
{
    return mUpdateFormat;
}

ResourceType::Changes FileResourceSettings::setUpdateFormat(bool update, bool sync)
{
    if (update == mUpdateFormat)
        return ResourceType::NoChange;

    mUpdateFormat = update;
    if (mConfigGroup)
        writeConfigUpdateFormat(sync);
    return ResourceType::UpdateFormat;
}

QByteArray FileResourceSettings::hash() const
{
    return mHash;
}

void FileResourceSettings::setHash(const QByteArray& hash, bool sync)
{
    if (hash != mHash)
    {
        mHash = hash;
        if (mConfigGroup)
            writeConfigHash(sync);
    }
}

QHash<QString, KAEvent::CmdErrType> FileResourceSettings::commandErrors() const
{
    return mCommandErrors;
}

void FileResourceSettings::setCommandErrors(const QHash<QString, KAEvent::CmdErrType>& cmdErrors, bool sync)
{
    if (cmdErrors != mCommandErrors)
    {
        mCommandErrors = cmdErrors;
        if (mConfigGroup)
            writeConfigCommandErrors(sync);
    }
}

/******************************************************************************
* Validate settings against each other, and amend to be consistent.
* Note that this validation cannot be done in the setXxxxx() methods, since
* they can be called when the settings configuration is incomplete.
*/
bool FileResourceSettings::validate()
{
    mEnabled  &= mAlarmTypes;
    mStandard &= mEnabled;
    mDisplayLocation = mUrl.toDisplayString(QUrl::PrettyDecoded | QUrl::PreferLocalFile);
    if (!(mAlarmTypes & CalEvent::ACTIVE))
        mCommandErrors.clear();
    if (storageType(mUrl) != mStorageType)
    {
        mStorageType = NoStorage;
        return false;
    }
    return true;
}

CalEvent::Types FileResourceSettings::readAlarmTypes(const char* key) const
{
    CalEvent::Types alarmTypes {CalEvent::EMPTY};
    const QStringList types = mConfigGroup->readEntry(key, QStringList());
    for (const QString& type : types)
    {
        if (type == ALARM_ACTIVE)
            alarmTypes |= CalEvent::ACTIVE;
        else if (type == ALARM_ARCHIVED)
            alarmTypes |= CalEvent::ARCHIVED;
        else if (type == ALARM_TEMPLATE)
            alarmTypes |= CalEvent::TEMPLATE;
    }
    return alarmTypes;
}

QString FileResourceSettings::alarmTypesString(CalEvent::Types alarmTypes)
{
    QStringList types;
    if (alarmTypes & CalEvent::ACTIVE)
        types += ALARM_ACTIVE;
    if (alarmTypes & CalEvent::ARCHIVED)
        types += ALARM_ARCHIVED;
    if (alarmTypes & CalEvent::TEMPLATE)
        types += ALARM_TEMPLATE;
    return types.join(QLatin1Char(','));
}

FileResourceSettings::StorageType FileResourceSettings::storageType(const QUrl& url)
{
    if (url.isLocalFile())
    {
        QFileInfo fi(url.toLocalFile());
        return (fi.exists() && fi.isDir()) ? Directory : File;
    }
    else
        return File;   // directory type not allowed if not a local file
}

void FileResourceSettings::writeConfigDisplayName(bool sync)
{
    mConfigGroup->writeEntry(KEY_NAME, mDisplayName);
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigAlarmTypes(bool sync)
{
    mConfigGroup->writeEntry(KEY_ALARMTYPES, alarmTypesString(mAlarmTypes));
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigEnabled(bool sync)
{
    mConfigGroup->writeEntry(KEY_ENABLED, alarmTypesString(mEnabled));
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigStandard(bool sync)
{
    mConfigGroup->writeEntry(KEY_STANDARD, alarmTypesString(mStandard));
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigBackgroundColour(bool sync)
{
    mConfigGroup->writeEntry(KEY_COLOUR, mBackgroundColour);
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigReadOnly(bool sync)
{
    mConfigGroup->writeEntry(KEY_READONLY, mReadOnly);
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigKeepFormat(bool sync)
{
    mConfigGroup->writeEntry(KEY_KEEPFORMAT, mKeepFormat);
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigUpdateFormat(bool sync)
{
    mConfigGroup->writeEntry(KEY_UPDATEFORMAT, mUpdateFormat);
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigHash(bool sync)
{
    mConfigGroup->writeEntry(KEY_HASH, mHash.toHex());
    if (sync)
        mConfigGroup->sync();
}

void FileResourceSettings::writeConfigCommandErrors(bool sync)
{
    QStringList cmdErrs;
    for (auto it = mCommandErrors.constBegin();  it != mCommandErrors.constEnd();  ++it)
    {
        QString type;
        switch (it.value())
        {
            case KAEvent::CMD_ERROR:          type = CMD_ERROR_VALUE;           break;
            case KAEvent::CMD_ERROR_PRE:      type = CMD_ERROR_PRE_VALUE;       break;
            case KAEvent::CMD_ERROR_POST:     type = CMD_ERROR_POST_VALUE;      break;
            case KAEvent::CMD_ERROR_PRE_POST: type = CMD_ERROR_PRE_POST_VALUE;  break;
            default:
                continue;
        }
        cmdErrs += it.key() + CMD_ERROR_SEPARATOR + type;
    }
    mConfigGroup->writeEntry(KEY_CMDERRORS, cmdErrs);
    if (sync)
        mConfigGroup->sync();
}

// vim: et sw=4:
