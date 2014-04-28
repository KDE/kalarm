/*
 *  preferences.cpp  -  program preference settings
 *  Program:  kalarm
 *  Copyright © 2001-2011 by David Jarvie <djarvie@kde.org>
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

#include "kalarm.h"

#include "functions.h"
#include "kamail.h"
#include "messagebox.h"
#include "preferences.h"

#include <kalarmcal/identities.h>

#include <KPIMIdentities/identity.h>
#include <KPIMIdentities/identitymanager.h>
#include <KHolidays/holidays.h>

#include <kglobal.h>
#include <kconfiggroup.h>
#include <kmessagebox.h>
#include <ksystemtimezone.h>
#include <kdebug.h>


#include <time.h>
#include <unistd.h>

using namespace KHolidays;
using namespace KAlarmCal;

// Config file entry names
static const char* GENERAL_SECTION  = "General";

// Config file entry name for temporary use
static const char* TEMP = "Temp";

// Values for EmailFrom entry
static const QString FROM_SYS_SETTINGS = QLatin1String("@SystemSettings");
static const QString FROM_KMAIL        = QLatin1String("@KMail");

// Config file entry names for notification messages
const QLatin1String Preferences::QUIT_WARN("QuitWarn");
const QLatin1String Preferences::ASK_AUTO_START("AskAutoStart");
const QLatin1String Preferences::CONFIRM_ALARM_DELETION("ConfirmAlarmDeletion");
const QLatin1String Preferences::EMAIL_QUEUED_NOTIFY("EmailQueuedNotify");
const bool  default_quitWarn             = true;
const bool  default_emailQueuedNotify    = false;
const bool  default_confirmAlarmDeletion = true;

static QString translateXTermPath(const QString& cmdline, bool write);


Preferences*   Preferences::mInstance = 0;
bool           Preferences::mUsingDefaults = false;
KTimeZone      Preferences::mSystemTimeZone;
HolidayRegion* Preferences::mHolidays = 0;   // always non-null after Preferences initialisation
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
    QObject::connect(this, SIGNAL(base_StartOfDayChanged(QDateTime)), SLOT(startDayChange(QDateTime)));
    QObject::connect(this, SIGNAL(base_TimeZoneChanged(QString)), SLOT(timeZoneChange(QString)));
    QObject::connect(this, SIGNAL(base_HolidayRegionChanged(QString)), SLOT(holidaysChange(QString)));
    QObject::connect(this, SIGNAL(base_WorkTimeChanged(QDateTime,QDateTime,int)), SLOT(workTimeChange(QDateTime,QDateTime,int)));

    readConfig();
    // Fetch the KAlarm version and backend which wrote the previous config file
    mPreviousVersion = version();
    mPreviousBackend = backend();
    // Update the KAlarm version in the config file, but don't call
    // writeConfig() here - leave it to be written only if the config file
    // is updated with other data.
    setVersion(QLatin1String(KALARM_VERSION));
}

void Preferences::setAskAutoStart(bool yes)
{
    KAMessageBox::saveDontShowAgainYesNo(ASK_AUTO_START, !yes);
}

/******************************************************************************
* Get the user's time zone, or if none has been chosen, the system time zone.
* The system time zone is cached, and the cached value will be returned unless
* 'reload' is true, in which case the value is re-read from the system.
*/
KTimeZone Preferences::timeZone(bool reload)
{
    if (reload)
        mSystemTimeZone = KTimeZone();
    QString timeZone = self()->mBase_TimeZone;
    KTimeZone tz;
    if (!timeZone.isEmpty())
        tz = KSystemTimeZones::zone(timeZone);
    if (!tz.isValid())
    {
        if (!mSystemTimeZone.isValid())
            mSystemTimeZone = KSystemTimeZones::local();
        tz = mSystemTimeZone;
    }
    return tz;
}

void Preferences::setTimeZone(const KTimeZone& tz)
{
    self()->setBase_TimeZone(tz.isValid() ? tz.name() : QString());
}

void Preferences::timeZoneChange(const QString& zone)
{
    Q_UNUSED(zone);
    emit mInstance->timeZoneChanged(timeZone(false));
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
    emit mInstance->holidaysChanged(holidays());
}

void Preferences::setStartOfDay(const QTime& t)
{
    if (t != self()->mBase_StartOfDay.time())
    {
        self()->setBase_StartOfDay(QDateTime(QDate(1900,1,1), t));
        emit mInstance->startOfDayChanged(t);
    }
}

// Called when the start of day value has changed in the config file
void Preferences::startDayChange(const QDateTime& dt)
{
    emit mInstance->startOfDayChanged(dt.time());
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
    emit mInstance->workTimeChanged(start.time(), end.time(), dayBits);
}

Preferences::MailFrom Preferences::emailFrom()
{
    QString from = self()->mBase_EmailFrom;
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
    QString from = self()->mBase_EmailFrom;
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
    QString from = self()->mBase_EmailBccAddress;
    if (from == FROM_SYS_SETTINGS)
        return MAIL_FROM_SYS_SETTINGS;
    return MAIL_FROM_ADDR;
}

QString Preferences::emailBccAddress()
{
    QString from = self()->mBase_EmailBccAddress;
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
    QChar quote = cmdline[0];
    char q = quote.toLatin1();
    bool quoted = (q == '"' || q == '\'');
    if (quoted)
        cmd = cmdline.mid(1);
    // Split the command at the first non-escaped space
    for (int i = 0, count = cmd.length();  i < count;  ++i)
    {
        switch (cmd[i].toLatin1())
        {
            case '\\':
                ++i;
                continue;
            case '"':
            case '\'':
                if (cmd[i] != quote)
                    continue;
                // fall through to ' '
            case ' ':
                params = cmd.mid(i);
                cmd = cmd.left(i);
                break;
            default:
                continue;
        }
        break;
    }
    // Translate any home directory specification at the start of the
    // executable's path.
    KConfigGroup group(KGlobal::config(), GENERAL_SECTION);
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
#include "moc_preferences.cpp"
// vim: et sw=4:
