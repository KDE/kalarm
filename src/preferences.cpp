/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  Copyright © 2001-2019 David Jarvie <djarvie@kde.org>
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

#include "preferences.h"

#include "kalarm.h"
#include "functions.h"
#include "kamail.h"
#include "lib/messagebox.h"
#include "kalarm_debug.h"

#include <KAlarmCal/Identities>

#include <KIdentityManagement/Identity>
#include <KIdentityManagement/IdentityManager>
#include <KHolidays/HolidayRegion>

#include <KSharedConfig>
#include <KConfigGroup>
#include <KMessageBox>

#include <QStandardPaths>

#include <time.h>
#include <unistd.h>

using namespace KHolidays;
using namespace KAlarmCal;

// Config file entry names
static const char* GENERAL_SECTION  = "General";

// Config file entry name for temporary use
static const char* TEMP = "Temp";

static const QString AUTOSTART_FILE(QStringLiteral("kalarm.autostart.desktop"));

// Values for EmailFrom entry
static const QString FROM_SYS_SETTINGS(QStringLiteral("@SystemSettings"));
static const QString FROM_KMAIL(QStringLiteral("@KMail"));

// Config file entry names for notification messages
const QLatin1String Preferences::QUIT_WARN("QuitWarn");
const QLatin1String Preferences::ASK_AUTO_START("AskAutoStart");
const QLatin1String Preferences::CONFIRM_ALARM_DELETION("ConfirmAlarmDeletion");
const QLatin1String Preferences::EMAIL_QUEUED_NOTIFY("EmailQueuedNotify");
const bool  default_quitWarn             = true;
const bool  default_emailQueuedNotify    = false;
const bool  default_confirmAlarmDeletion = true;

static QString translateXTermPath(const QString& cmdline, bool write);


Preferences*   Preferences::mInstance = nullptr;
bool           Preferences::mUsingDefaults = false;
HolidayRegion* Preferences::mHolidays = nullptr;   // always non-null after Preferences initialisation
QString        Preferences::mPreviousVersion;
Preferences::Backend Preferences::mPreviousBackend;
// Change tracking
bool           Preferences::mAutoStartChangedByUser = false;


Preferences* Preferences::self()
{
    if (!mInstance)
    {
        // Set the default button for the Quit warning message box to Cancel
        KAMessageBox::setContinueDefault(QUIT_WARN, KMessageBox::Cancel);
        KAMessageBox::setDefaultShouldBeShownContinue(QUIT_WARN, default_quitWarn);
        KAMessageBox::setDefaultShouldBeShownContinue(EMAIL_QUEUED_NOTIFY, default_emailQueuedNotify);
        KAMessageBox::setDefaultShouldBeShownContinue(CONFIRM_ALARM_DELETION, default_confirmAlarmDeletion);

        mInstance = new Preferences;
    }
    return mInstance;
}

Preferences::Preferences()
{
    QObject::connect(this, &Preferences::base_StartOfDayChanged, this, &Preferences::startDayChange);
    QObject::connect(this, &Preferences::base_TimeZoneChanged, this, &Preferences::timeZoneChange);
    QObject::connect(this, &Preferences::base_HolidayRegionChanged, this, &Preferences::holidaysChange);
    QObject::connect(this, &Preferences::base_WorkTimeChanged, this, &Preferences::workTimeChange);

    load();
    // Fetch the KAlarm version and backend which wrote the previous config file
    mPreviousVersion = version();
    mPreviousBackend = backend();
    // Update the KAlarm version in the config file, but don't call
    // writeConfig() here - leave it to be written only if the config file
    // is updated with other data.
    setVersion(QStringLiteral(KALARM_VERSION));
}

/******************************************************************************
* Auto hiding of the system tray icon is only allowed on desktops which provide
* GUI controls to show hidden icons.
*/
int Preferences::autoHideSystemTray()
{
    if(noAutoHideSystemTrayDesktops().contains(KAlarm::currentDesktopIdentityName()))
        return 0;   // never hide
    return self()->mBase_AutoHideSystemTray;
}

/******************************************************************************
* Auto hiding of the system tray icon is only allowed on desktops which provide
* GUI controls to show hidden icons, so while KAlarm is running on such a
* desktop, don't allow changes to the setting.
*/
void Preferences::setAutoHideSystemTray(int timeout)
{
    if(noAutoHideSystemTrayDesktops().contains(KAlarm::currentDesktopIdentityName()))
        return;
    self()->setBase_AutoHideSystemTray(timeout);
}

void Preferences::setAskAutoStart(bool yes)
{
    KAMessageBox::saveDontShowAgainYesNo(ASK_AUTO_START, !yes);
}

/******************************************************************************
* Set the NoAutoStart condition.
* On KDE desktops, the "X-KDE-autostart-condition" entry in
* kalarm.autostart.desktop references this to determine whether to autostart KAlarm.
* On non-KDE desktops, the "X-KDE-autostart-condition" entry in
* kalarm.autostart.desktop doesn't have any effect, so that KAlarm will be
* autostarted even if it is set not to autostart. Adding a "Hidden" entry to,
* and removing the "OnlyShowIn=KDE" entry from, a user-modifiable copy of the
* file fixes this.
*/
void Preferences::setNoAutoStart(bool yes)
{
    // Find the existing kalarm.autostart.desktop file, and whether it's writable.
    bool existingRO = true;   // whether the existing file is read-only
    QString autostartFile;
    const QStringList autostartDirs = QStandardPaths::standardLocations(QStandardPaths::GenericConfigLocation);
    for (const QString& dir : autostartDirs)
    {
        const QString file = dir + QLatin1String("/autostart/") + AUTOSTART_FILE;
        if (QFile::exists(file))
        {
            QFileInfo info(file);
            if (info.isReadable())
            {
                autostartFile = file;
                existingRO = !info.isWritable();
                break;
            }
        }
    }

    // If the existing file isn't writable, find the path to create a writable copy
    QString autostartFileRW = autostartFile;
    QString configDirRW;
    if (existingRO)
    {
        configDirRW = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
        autostartFileRW = configDirRW + QLatin1String("/autostart/") + AUTOSTART_FILE;
        if (configDirRW.isEmpty())
        {
            qCWarning(KALARM_LOG) << "Preferences::setNoAutoStart: No writable autostart file path";
            return;
        }
        if (QFile::exists(autostartFileRW))
        {
            QFileInfo info(autostartFileRW);
            if (!info.isReadable() || !info.isWritable())
            {
                qCWarning(KALARM_LOG) << "Preferences::setNoAutoStart: Autostart file is not read/write:" << autostartFileRW;
                return;
            }
        }
    }

    // Read the existing file and remove any "Hidden=" and "OnlyShowIn=" entries
    bool update = false;
    QStringList lines;
    {
        QFile file(autostartFile);
        if (!file.open(QIODevice::ReadOnly))
        {
            qCWarning(KALARM_LOG) << "Preferences::setNoAutoStart: Error reading autostart file:" << autostartFile;
            return;
        }
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream.setAutoDetectUnicode(true);
        lines = stream.readAll().split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i)
        {
            const QString line = lines.at(i).trimmed();
            if (line.isEmpty())
            {
                lines.removeAt(i);
                --i;
            }
            else if (line.startsWith(QLatin1String("Hidden="))
                 ||  line.startsWith(QLatin1String("OnlyShowIn=")))
            {
                lines.removeAt(i);
                update = true;
                --i;
            }
        }
    }

    if (yes)
    {
        // Add a "Hidden" entry to the local kalarm.autostart.desktop file, to
        // prevent autostart from happening.
        lines += QStringLiteral("Hidden=true");
        update = true;
    }
    if (update)
    {
        // Write the updated file
        QFileInfo info(configDirRW + QLatin1String("/autostart"));
        if (!info.exists())
        {
            // First, create the directory for it.
            if (!QDir(configDirRW).mkdir(QStringLiteral("autostart")))
            {
                qCWarning(KALARM_LOG) << "Preferences::setNoAutoStart: Error creating autostart file directory:" << info.filePath();
                return;
            }
        }
        QFile file(autostartFileRW);
        if (!file.open(QIODevice::WriteOnly))
        {
            qCWarning(KALARM_LOG) << "Preferences::setNoAutoStart: Error writing autostart file:" << autostartFileRW;
            return;
        }
        QTextStream stream(&file);
        stream.setCodec("UTF-8");
        stream << lines.join(QLatin1Char('\n')) << "\n";
        qCDebug(KALARM_LOG) << "Preferences::setNoAutoStart: Written" << autostartFileRW;
    }

    self()->setBase_NoAutoStart(yes);
}

/******************************************************************************
* Get the user's time zone, or if none has been chosen, the system time zone.
* Reply = time zone, or invalid to use the local time zone.
*/
KADateTime::Spec Preferences::timeSpec()
{
    const QByteArray zoneId = self()->mBase_TimeZone.toLatin1();
    return zoneId.isEmpty() ? KADateTime::LocalZone : KADateTime::Spec(QTimeZone(zoneId));
}

QTimeZone Preferences::timeSpecAsZone()
{
    const QByteArray zoneId = self()->mBase_TimeZone.toLatin1();
    return zoneId.isEmpty() ? QTimeZone::systemTimeZone() : QTimeZone(zoneId);
}

void Preferences::setTimeSpec(const KADateTime::Spec& spec)
{
    self()->setBase_TimeZone(spec.type() == KADateTime::TimeZone ? QString::fromLatin1(spec.timeZone().id()) : QString());
}

void Preferences::timeZoneChange(const QString& zone)
{
    Q_UNUSED(zone);
    Q_EMIT mInstance->timeZoneChanged(timeSpec());
}

const HolidayRegion& Preferences::holidays()
{
    QString regionCode = self()->mBase_HolidayRegion;
    if (!mHolidays  ||  mHolidays->regionCode() != regionCode)
    {
        delete mHolidays;
        mHolidays = new HolidayRegion(regionCode);
    }
    return *mHolidays;
}

void Preferences::setHolidayRegion(const QString& regionCode)
{
    self()->setBase_HolidayRegion(regionCode);
}

void Preferences::holidaysChange(const QString& regionCode)
{
    Q_UNUSED(regionCode);
    Q_EMIT mInstance->holidaysChanged(holidays());
}

void Preferences::setStartOfDay(const QTime& t)
{
    if (t != self()->mBase_StartOfDay.time())
    {
        self()->setBase_StartOfDay(QDateTime(QDate(1900,1,1), t));
        Q_EMIT mInstance->startOfDayChanged(t);
    }
}

// Called when the start of day value has changed in the config file
void Preferences::startDayChange(const QDateTime& dt)
{
    Q_EMIT mInstance->startOfDayChanged(dt.time());
}

QBitArray Preferences::workDays()
{
    unsigned days = self()->base_WorkDays();
    QBitArray dayBits(7);
    for (int i = 0;  i < 7;  ++i)
        dayBits.setBit(i, days & (1 << i));
    return dayBits;
}

void Preferences::setWorkDays(const QBitArray& dayBits)
{
    if (dayBits.size() != 7)
    {
        qCWarning(KALARM_LOG) << "Preferences::setWorkDays: Error! 'dayBits' parameter must have 7 elements: actual size" << dayBits.size();
        return;
    }
    unsigned days = 0;
    for (int i = 0;  i < 7;  ++i)
        if (dayBits.testBit(i))
            days |= 1 << i;
    self()->setBase_WorkDays(days);
}

void Preferences::workTimeChange(const QDateTime& start, const QDateTime& end, int days)
{
    QBitArray dayBits(7);
    for (int i = 0;  i < 7;  ++i)
        if (days & (1 << i))
            dayBits.setBit(i);
    Q_EMIT mInstance->workTimeChanged(start.time(), end.time(), dayBits);
}

Preferences::MailFrom Preferences::emailFrom()
{
    const QString from = self()->mBase_EmailFrom;
    if (from == FROM_KMAIL)
        return MAIL_FROM_KMAIL;
    if (from == FROM_SYS_SETTINGS)
        return MAIL_FROM_SYS_SETTINGS;
    return MAIL_FROM_ADDR;
}

/******************************************************************************
* Get user's default 'From' email address.
*/
QString Preferences::emailAddress()
{
    const QString from = self()->mBase_EmailFrom;
    if (from == FROM_KMAIL)
        return Identities::identityManager()->defaultIdentity().fullEmailAddr();
    if (from == FROM_SYS_SETTINGS)
        return KAMail::controlCentreAddress();
    return from;
}

void Preferences::setEmailAddress(Preferences::MailFrom from, const QString& address)
{
    QString out;
    switch (from)
    {
        case MAIL_FROM_KMAIL:        out = FROM_KMAIL; break;
        case MAIL_FROM_SYS_SETTINGS: out = FROM_SYS_SETTINGS; break;
        case MAIL_FROM_ADDR:         out = address; break;
        default:  return;
    }
    self()->setBase_EmailFrom(out);
}

Preferences::MailFrom Preferences::emailBccFrom()
{
    const QString from = self()->mBase_EmailBccAddress;
    if (from == FROM_SYS_SETTINGS)
        return MAIL_FROM_SYS_SETTINGS;
    return MAIL_FROM_ADDR;
}

QString Preferences::emailBccAddress()
{
    const QString from = self()->mBase_EmailBccAddress;
    if (from == FROM_SYS_SETTINGS)
        return KAMail::controlCentreAddress();
    return from;
}

bool Preferences::emailBccUseSystemSettings()
{
    return self()->mBase_EmailBccAddress == FROM_SYS_SETTINGS;
}

void Preferences::setEmailBccAddress(bool useSystemSettings, const QString& address)
{
    QString out;
    if (useSystemSettings)
        out = FROM_SYS_SETTINGS;
    else
        out = address;
    self()->setBase_EmailBccAddress(out);
}

QString Preferences::cmdXTermCommand()
{
    return translateXTermPath(self()->mBase_CmdXTermCommand, false);
}

void Preferences::setCmdXTermCommand(const QString& cmd)
{
    self()->setBase_CmdXTermCommand(translateXTermPath(cmd, true));
}


void Preferences::connect(const char* signal, const QObject* receiver, const char* member)
{
    QObject::connect(self(), signal, receiver, member);
}

/******************************************************************************
* Called to allow or suppress output of the specified message dialog, where the
* dialog has a checkbox to turn notification off.
*/
void Preferences::setNotify(const QString& messageID, bool notify)
{
    KAMessageBox::saveDontShowAgainContinue(messageID, !notify);
}

/******************************************************************************
* Return whether the specified message dialog is output, where the dialog has
* a checkbox to turn notification off.
* Reply = false if message has been suppressed (by preferences or by selecting
*               "don't ask again")
*       = true in all other cases.
*/
bool Preferences::notifying(const QString& messageID)
{
    return KAMessageBox::shouldBeShownContinue(messageID);
}

/******************************************************************************
* Translate an X terminal command path to/from config file format.
* Note that only a home directory specification at the start of the path is
* translated, so there's no need to worry about missing out some of the
* executable's path due to quotes etc.
* N.B. Calling KConfig::read/writePathEntry() on the entire command line
*      causes a crash on some systems, so it's necessary to extract the
*      executable path first before processing.
*/
QString translateXTermPath(const QString& cmdline, bool write)
{
    QString params;
    QString cmd = cmdline;
    if (cmdline.isEmpty())
        return cmdline;
    // Strip any leading quote
    const QChar quote = cmdline[0];
    const char q = quote.toLatin1();
    const bool quoted = (q == '"' || q == '\'');
    if (quoted)
        cmd = cmdline.mid(1);
    // Split the command at the first non-escaped space
    for (int i = 0, count = cmd.length();  i < count;  ++i)
    {
        switch (cmd.at(i).toLatin1())
        {
            case '\\':
                ++i;
                continue;
            case '"':
            case '\'':
                if (cmd.at(i) != quote)
                    continue;
                // fall through to ' '
                Q_FALLTHROUGH();
            case ' ':
                params = cmd.mid(i);
                cmd.truncate(i);
                break;
            default:
                continue;
        }
        break;
    }
    // Translate any home directory specification at the start of the
    // executable's path.
    KConfigGroup group(KSharedConfig::openConfig(), GENERAL_SECTION);
    if (write)
    {
        group.writePathEntry(TEMP, cmd);
        cmd = group.readEntry(TEMP, QString());
    }
    else
    {
        group.writeEntry(TEMP, cmd);
        cmd = group.readPathEntry(TEMP, QString());
    }
    group.deleteEntry(TEMP);
    if (quoted)
        return quote + cmd + params;
    else
        return cmd + params;
}

// vim: et sw=4:
