/*
 *  dbushandler.cpp  -  handler for D-Bus calls by other applications
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2002-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dbushandler.h"

#include "functions.h"
#include "kalarmapp.h"
#include "kamail.h"
#include "resources/resources.h"
#include <kalarmadaptor.h>
#include "kalarmcalendar/identities.h"
#include "kalarmcalendar/karecurrence.h"
#include "kalarm_debug.h"

using namespace KCalendarCore;

#include <stdlib.h>

//clazy:excludeall=non-pod-global-static

namespace
{
const QString REQUEST_DBUS_OBJECT(QStringLiteral("/kalarm"));   // D-Bus object path of KAlarm's request interface
}


/*=============================================================================
= DBusHandler
= This class's function is to handle D-Bus requests by other applications.
=============================================================================*/
DBusHandler::DBusHandler()
{
    qCDebug(KALARM_LOG) << "DBusHandler:";
    new KalarmAdaptor(this);
    QDBusConnection::sessionBus().registerObject(REQUEST_DBUS_OBJECT, this);
}


bool DBusHandler::cancelEvent(const QString& eventId)
{
    if (!Resources::allPopulated())
        return false;    // can't access events before calendars are loaded
    return theApp()->dbusDeleteEvent(EventId(eventId));
}

bool DBusHandler::triggerEvent(const QString& eventId)
{
    if (!Resources::allPopulated())
        return false;    // can't access events before calendars are loaded
    return theApp()->dbusTriggerEvent(EventId(eventId));
}

QString DBusHandler::list()
{
    return theApp()->dbusList();
}

bool DBusHandler::scheduleMessage(const QString& name, const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins, const QString& recurrence,
                                  int subRepeatInterval, int subRepeatCount)
{
    KADateTime start;
    KARecurrence recur;
    Duration subRepeatDuration;
    if (!convertRecurrence(start, recur, startDateTime, recurrence, subRepeatInterval, subRepeatDuration))
        return false;
    return scheduleMessage(name, message, start, lateCancel, flags, bgColor, fgColor, font,
                           QUrl::fromUserInput(audioUrl, QString(), QUrl::AssumeLocalFile),
                           reminderMins, recur, subRepeatDuration, subRepeatCount);
}

bool DBusHandler::scheduleMessage(const QString& name, const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins,
                                  int recurType, int recurInterval, int recurCount)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
        return false;
    return scheduleMessage(name, message, start, lateCancel, flags, bgColor, fgColor, font,
                           QUrl::fromUserInput(audioUrl, QString(), QUrl::AssumeLocalFile),
                           reminderMins, recur);
}

bool DBusHandler::scheduleMessage(const QString& name, const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
        return false;
    return scheduleMessage(name, message, start, lateCancel, flags, bgColor, fgColor, font,
                           QUrl::fromUserInput(audioUrl, QString(), QUrl::AssumeLocalFile),
                           reminderMins, recur);
}

bool DBusHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins, const QString& recurrence,
                                  int subRepeatInterval, int subRepeatCount)
{
    return scheduleMessage(QString(), message, startDateTime, lateCancel, flags, bgColor, fgColor, font,
                           audioUrl, reminderMins, recurrence, subRepeatInterval, subRepeatCount);
}

bool DBusHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins,
                                  int recurType, int recurInterval, int recurCount)
{
    return scheduleMessage(QString(), message, startDateTime, lateCancel, flags, bgColor, fgColor, font,
                           audioUrl, reminderMins, recurType, recurInterval, recurCount);
}

bool DBusHandler::scheduleMessage(const QString& message, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& font,
                                  const QString& audioUrl, int reminderMins,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
    return scheduleMessage(QString(), message, startDateTime, lateCancel, flags, bgColor, fgColor, font,
                           audioUrl, reminderMins, recurType, recurInterval, endDateTime);
}

bool DBusHandler::scheduleFile(const QString& name, const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, const QString& recurrence,
                               int subRepeatInterval, int subRepeatCount)
{
    KADateTime start;
    KARecurrence recur;
    Duration subRepeatDuration;
    if (!convertRecurrence(start, recur, startDateTime, recurrence, subRepeatInterval, subRepeatDuration))
        return false;
    return scheduleFile(name, QUrl::fromUserInput(url, QString(), QUrl::AssumeLocalFile),
                        start, lateCancel, flags, bgColor,
                        QUrl::fromUserInput(audioUrl, QString(), QUrl::AssumeLocalFile),
                        reminderMins, recur, subRepeatDuration, subRepeatCount);
}

bool DBusHandler::scheduleFile(const QString& name, const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
        return false;
    return scheduleFile(name, QUrl::fromUserInput(url, QString(), QUrl::AssumeLocalFile),
                        start, lateCancel, flags, bgColor,
                        QUrl::fromUserInput(audioUrl, QString(), QUrl::AssumeLocalFile),
                        reminderMins, recur);
}

bool DBusHandler::scheduleFile(const QString& name, const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
        return false;
    return scheduleFile(name, QUrl::fromUserInput(url, QString(), QUrl::AssumeLocalFile),
                        start, lateCancel, flags, bgColor,
                        QUrl::fromUserInput(audioUrl, QString(), QUrl::AssumeLocalFile),
                        reminderMins, recur);
}

bool DBusHandler::scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, const QString& recurrence,
                               int subRepeatInterval, int subRepeatCount)
{
    return scheduleFile(QString(), url, startDateTime, lateCancel, flags, bgColor,
                        audioUrl, reminderMins, recurrence, subRepeatInterval, subRepeatCount);
}

bool DBusHandler::scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, int recurType, int recurInterval, int recurCount)
{
    return scheduleFile(QString(), url, startDateTime, lateCancel, flags, bgColor,
                        audioUrl, reminderMins, recurType, recurInterval, recurCount);
}

bool DBusHandler::scheduleFile(const QString& url, const QString& startDateTime, int lateCancel, unsigned flags, const QString& bgColor,
                               const QString& audioUrl, int reminderMins, int recurType, int recurInterval, const QString& endDateTime)
{
    return scheduleFile(QString(), url, startDateTime, lateCancel, flags, bgColor,
                        audioUrl, reminderMins, recurType, recurInterval, endDateTime);
}

bool DBusHandler::scheduleCommand(const QString& name, const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& recurrence, int subRepeatInterval, int subRepeatCount)
{
    KADateTime start;
    KARecurrence recur;
    Duration subRepeatDuration;
    if (!convertRecurrence(start, recur, startDateTime, recurrence, subRepeatInterval, subRepeatDuration))
        return false;
    return scheduleCommand(name, commandLine, start, lateCancel, flags, recur, subRepeatDuration, subRepeatCount);
}

bool DBusHandler::scheduleCommand(const QString& name, const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, int recurCount)
{
    KADateTime start = convertDateTime(startDateTime);
    if (!start.isValid())
        return false;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
        return false;
    return scheduleCommand(name, commandLine, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleCommand(const QString& name, const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
        return false;
    return scheduleCommand(name, commandLine, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  const QString& recurrence, int subRepeatInterval, int subRepeatCount)
{
    return scheduleCommand(QString(), commandLine, startDateTime, lateCancel, flags, recurrence, subRepeatInterval, subRepeatCount);
}

bool DBusHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, int recurCount)
{
    return scheduleCommand(QString(), commandLine, startDateTime, lateCancel, flags, recurType, recurInterval, recurCount);
}

bool DBusHandler::scheduleCommand(const QString& commandLine, const QString& startDateTime, int lateCancel, unsigned flags,
                                  int recurType, int recurInterval, const QString& endDateTime)
{
    return scheduleCommand(QString(), commandLine, startDateTime, lateCancel, flags, recurType, recurInterval, endDateTime);
}

bool DBusHandler::scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                const QString& recurrence, int subRepeatInterval, int subRepeatCount)
{
    KADateTime start;
    KARecurrence recur;
    Duration subRepeatDuration;
    if (!convertRecurrence(start, recur, startDateTime, recurrence, subRepeatInterval, subRepeatDuration))
        return false;
    return scheduleEmail(name, fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur, subRepeatDuration, subRepeatCount);
}

bool DBusHandler::scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, int recurCount)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
        return false;
    return scheduleEmail(name, fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, const QString& endDateTime)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
        return false;
    return scheduleEmail(name, fromID, addresses, subject, message, attachments, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                const QString& recurrence, int subRepeatInterval, int subRepeatCount)
{
    return scheduleEmail(QString(), fromID, addresses, subject, message, attachments, startDateTime, lateCancel, flags, recurrence, subRepeatInterval, subRepeatCount);
}

bool DBusHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, int recurCount)
{
    return scheduleEmail(QString(), fromID, addresses, subject, message, attachments, startDateTime, lateCancel, flags, recurType, recurInterval, recurCount);
}

bool DBusHandler::scheduleEmail(const QString& fromID, const QString& addresses, const QString& subject, const QString& message,
                                const QString& attachments, const QString& startDateTime, int lateCancel, unsigned flags,
                                int recurType, int recurInterval, const QString& endDateTime)
{
    return scheduleEmail(QString(), fromID, addresses, subject, message, attachments, startDateTime, lateCancel, flags, recurType, recurInterval, endDateTime);
}

bool DBusHandler::scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                unsigned flags, const QString& recurrence, int subRepeatInterval, int subRepeatCount)
{
    KADateTime start;
    KARecurrence recur;
    Duration subRepeatDuration;
    if (!convertRecurrence(start, recur, startDateTime, recurrence, subRepeatInterval, subRepeatDuration))
        return false;
    return scheduleAudio(name, audioUrl, volumePercent, start, lateCancel, flags, recur, subRepeatDuration, subRepeatCount);
}

bool DBusHandler::scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                unsigned flags, int recurType, int recurInterval, int recurCount)
{
    KADateTime start = convertDateTime(startDateTime);
    if (!start.isValid())
        return false;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, recurCount))
        return false;
    return scheduleAudio(name, audioUrl, volumePercent, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                unsigned flags, int recurType, int recurInterval, const QString& endDateTime)
{
    KADateTime start;
    KARecurrence recur;
    if (!convertRecurrence(start, recur, startDateTime, recurType, recurInterval, endDateTime))
        return false;
    return scheduleAudio(name, audioUrl, volumePercent, start, lateCancel, flags, recur);
}

bool DBusHandler::scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                unsigned flags, const QString& recurrence, int subRepeatInterval, int subRepeatCount)
{
    return scheduleAudio(QString(), audioUrl, volumePercent, startDateTime, lateCancel, flags, recurrence, subRepeatInterval, subRepeatCount);
}

bool DBusHandler::scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                unsigned flags, int recurType, int recurInterval, int recurCount)
{
    return scheduleAudio(QString(), audioUrl, volumePercent, startDateTime, lateCancel, flags, recurType, recurInterval, recurCount);
}

bool DBusHandler::scheduleAudio(const QString& audioUrl, int volumePercent, const QString& startDateTime, int lateCancel,
                                unsigned flags, int recurType, int recurInterval, const QString& endDateTime)
{
    return scheduleAudio(QString(), audioUrl, volumePercent, startDateTime, lateCancel, flags, recurType, recurInterval, endDateTime);
}

bool DBusHandler::edit(const QString& eventID)
{
    if (!Resources::allPopulated())
        return false;    // can't access events before calendars are loaded
    return KAlarm::editAlarmById(EventId(eventID));
}

bool DBusHandler::editNew(int type)
{
    EditAlarmDlg::Type dlgtype;
    switch (type)
    {
        case DISPLAY:  dlgtype = EditAlarmDlg::DISPLAY;  break;
        case COMMAND:  dlgtype = EditAlarmDlg::COMMAND;  break;
        case EMAIL:    dlgtype = EditAlarmDlg::EMAIL;  break;
        case AUDIO:    dlgtype = EditAlarmDlg::AUDIO;  break;
        default:
            qCCritical(KALARM_LOG) << "D-Bus call: invalid alarm type:" << type;
            return false;
    }
    KAlarm::editNewAlarm(dlgtype);
    return true;
}

bool DBusHandler::editNew(const QString& templateName)
{
    return KAlarm::editNewAlarm(templateName);
}


/******************************************************************************
* Schedule a message alarm, after converting the parameters from strings.
*/
bool DBusHandler::scheduleMessage(const QString& name, const QString& message, const KADateTime& start, int lateCancel, unsigned flags,
                                  const QString& bgColor, const QString& fgColor, const QString& fontStr,
                                  const QUrl& audioFile, int reminderMins, const KARecurrence& recurrence,
                                  const KCalendarCore::Duration& subRepeatDuration, int subRepeatCount)
{
    KAEvent::Flags kaEventFlags = convertStartFlags(start, flags);
    const KAEvent::SubAction action = (kaEventFlags & KAEvent::DISPLAY_COMMAND) ? KAEvent::SubAction::Command : KAEvent::SubAction::Message;
    const QColor bg = convertBgColour(bgColor);
    if (!bg.isValid())
        return false;
    QColor fg;
    if (fgColor.isEmpty())
        fg = Preferences::defaultFgColour();
    else
    {
        fg.setNamedColor(fgColor);
        if (!fg.isValid())
        {
            qCCritical(KALARM_LOG) << "D-Bus call: invalid foreground color:" << fgColor;
            return false;
        }
    }
    QFont font;
    if (fontStr.isEmpty())
        kaEventFlags |= KAEvent::DEFAULT_FONT;
    else
    {
        if (!font.fromString(fontStr))    // N.B. this doesn't do good validation
        {
            qCCritical(KALARM_LOG) << "D-Bus call: invalid font:" << fontStr;
            return false;
        }
    }
    return theApp()->scheduleEvent(action, name, message, start, lateCancel, kaEventFlags, bg, fg, font,
                                   audioFile.toString(), -1, reminderMins, recurrence, subRepeatDuration, subRepeatCount);
}

/******************************************************************************
* Schedule a file alarm, after converting the parameters from strings.
*/
bool DBusHandler::scheduleFile(const QString& name, const QUrl& file,
                               const KADateTime& start, int lateCancel, unsigned flags, const QString& bgColor,
                               const QUrl& audioFile, int reminderMins, const KARecurrence& recurrence,
                               const KCalendarCore::Duration& subRepeatDuration, int subRepeatCount)
{
    const KAEvent::Flags kaEventFlags = convertStartFlags(start, flags);
    const QColor bg = convertBgColour(bgColor);
    if (!bg.isValid())
        return false;
    return theApp()->scheduleEvent(KAEvent::SubAction::File, name, file.toString(), start, lateCancel, kaEventFlags, bg, Qt::black, QFont(),
                                   audioFile.toString(), -1, reminderMins, recurrence, subRepeatDuration, subRepeatCount);
}

/******************************************************************************
* Schedule a command alarm, after converting the parameters from strings.
*/
bool DBusHandler::scheduleCommand(const QString& name, const QString& commandLine,
                                  const KADateTime& start, int lateCancel, unsigned flags,
                                  const KARecurrence& recurrence, const KCalendarCore::Duration& subRepeatDuration, int subRepeatCount)
{
    const KAEvent::Flags kaEventFlags = convertStartFlags(start, flags);
    return theApp()->scheduleEvent(KAEvent::SubAction::Command, name, commandLine, start, lateCancel, kaEventFlags, Qt::black, Qt::black, QFont(),
                                   QString(), -1, 0, recurrence, subRepeatDuration, subRepeatCount);
}

/******************************************************************************
* Schedule an email alarm, after validating the addresses and attachments.
*/
bool DBusHandler::scheduleEmail(const QString& name, const QString& fromID, const QString& addresses, const QString& subject,
                                const QString& message, const QString& attachments,
                                const KADateTime& start, int lateCancel, unsigned flags,
                                const KARecurrence& recurrence, const KCalendarCore::Duration& subRepeatDuration, int subRepeatCount)
{
    const KAEvent::Flags kaEventFlags = convertStartFlags(start, flags);
    uint senderId = 0;
    if (!fromID.isEmpty())
    {
        senderId = Identities::identityUoid(fromID);
        if (!senderId)
        {
            qCCritical(KALARM_LOG) << "D-Bus call scheduleEmail(): unknown sender ID:" << fromID;
            return false;
        }
    }
    KCalendarCore::Person::List addrs;
    QString bad = KAMail::convertAddresses(addresses, addrs);
    if (!bad.isEmpty())
    {
        qCCritical(KALARM_LOG) << "D-Bus call scheduleEmail(): invalid email addresses:" << bad;
        return false;
    }
    if (addrs.isEmpty())
    {
        qCCritical(KALARM_LOG) << "D-Bus call scheduleEmail(): no email address";
        return false;
    }
    QStringList atts;
    bad = KAMail::convertAttachments(attachments, atts);
    if (!bad.isEmpty())
    {
        qCCritical(KALARM_LOG) << "D-Bus call scheduleEmail(): invalid email attachment:" << bad;
        return false;
    }
    return theApp()->scheduleEvent(KAEvent::SubAction::Email, name, message, start, lateCancel, kaEventFlags, Qt::black, Qt::black, QFont(),
                                   QString(), -1, 0, recurrence, subRepeatDuration, subRepeatCount, senderId, addrs, subject, atts);
}

/******************************************************************************
* Schedule a audio alarm, after converting the parameters from strings.
*/
bool DBusHandler::scheduleAudio(const QString& name, const QString& audioUrl, int volumePercent,
                                const KADateTime& start, int lateCancel, unsigned flags,
                                const KARecurrence& recurrence, const KCalendarCore::Duration& subRepeatDuration, int subRepeatCount)
{
    const KAEvent::Flags kaEventFlags = convertStartFlags(start, flags);
    const float volume = (volumePercent >= 0) ? volumePercent / 100.0f : -1;
    return theApp()->scheduleEvent(KAEvent::SubAction::Audio, name, QString(), start, lateCancel, kaEventFlags, Qt::black, Qt::black, QFont(),
                                   audioUrl, volume, 0, recurrence, subRepeatDuration, subRepeatCount);
}


/******************************************************************************
* Convert the start date/time string to a KADateTime. The date/time string is in
* the format YYYY-MM-DD[THH:MM[:SS]][ TZ] or [T]HH:MM[:SS].
* The time zone specifier (TZ) is a system time zone name, e.g. "Europe/London".
* If no time zone is specified, it defaults to the local clock time (which is
* not the same as the local time zone).
*/
KADateTime DBusHandler::convertDateTime(const QString& dateTime, const KADateTime& defaultDt)
{
    int i = dateTime.indexOf(QLatin1Char(' '));
    QString dtString = dateTime;
    QString zone;
    if (i >= 0)
    {
        dtString = dateTime.left(i);
        zone     = dateTime.mid(i);
    }
    QDate date;
    QTime time;
    bool haveTime = true;
    bool error = false;
    if (dtString.length() > 10)
    {
        // Both a date and a time are specified
        const QDateTime dt = QDateTime::fromString(dtString, Qt::ISODate);
        error = !dt.isValid();
        date = dt.date();
        time = dt.time();
    }
    else
    {
        // Check whether a time is specified
        QString t;
        if (dtString[0] == QLatin1Char('T'))
            t = dtString.mid(1);     // it's a time: remove the leading 'T'
        else if (!dtString[2].isDigit())
            t = dtString;            // it's a time with no leading 'T'

        if (t.isEmpty())
        {
            // It's a date only
            date = QDate::fromString(dtString, Qt::ISODate);
            error = !date.isValid();
            haveTime = false;
        }
        else
        {
            // It's a time only
            time = QTime::fromString(t, Qt::ISODate);
            error = !time.isValid();
        }
    }
    KADateTime result;
    if (!error)
        result = KAlarm::applyTimeZone(zone, date, time, haveTime, defaultDt);
    if (error  ||  !result.isValid())
    {
        if (!defaultDt.isValid())
            qCCritical(KALARM_LOG) << "D-Bus call: invalid start date/time: '" << dateTime << "'";
        else
            qCCritical(KALARM_LOG) << "D-Bus call: invalid recurrence end date/time: '" << dateTime << "'";
    }
    return result;
}

/******************************************************************************
* Convert the flag bits to KAEvent flag bits.
*/
KAEvent::Flags DBusHandler::convertStartFlags(const KADateTime& start, unsigned flags)
{
    KAEvent::Flags kaEventFlags = {};
    if (flags & REPEAT_AT_LOGIN) kaEventFlags |= KAEvent::REPEAT_AT_LOGIN;
    if (flags & BEEP)            kaEventFlags |= KAEvent::BEEP;
    if (flags & SPEAK)           kaEventFlags |= KAEvent::SPEAK;
    if (flags & CONFIRM_ACK)     kaEventFlags |= KAEvent::CONFIRM_ACK;
    if (flags & REPEAT_SOUND)    kaEventFlags |= KAEvent::REPEAT_SOUND;
    if (flags & AUTO_CLOSE)      kaEventFlags |= KAEvent::AUTO_CLOSE;
    if (flags & EMAIL_BCC)       kaEventFlags |= KAEvent::EMAIL_BCC;
    if (flags & DISPLAY_COMMAND) kaEventFlags |= KAEvent::DISPLAY_COMMAND;
    if (flags & SCRIPT)          kaEventFlags |= KAEvent::SCRIPT;
    if (flags & EXEC_IN_XTERM)   kaEventFlags |= KAEvent::EXEC_IN_XTERM;
    if (flags & SHOW_IN_KORG)    kaEventFlags |= KAEvent::COPY_KORGANIZER;
    if (flags & EXCL_HOLIDAYS)   kaEventFlags |= KAEvent::EXCL_HOLIDAYS;
    if (flags & WORK_TIME_ONLY)  kaEventFlags |= KAEvent::WORK_TIME_ONLY;
    if (flags & DISABLED)        kaEventFlags |= KAEvent::DISABLED;
    if (flags & NOTIFY)          kaEventFlags |= KAEvent::NOTIFY;
    if (start.isDateOnly())      kaEventFlags |= KAEvent::ANY_TIME;
    return kaEventFlags;
}

/******************************************************************************
* Convert the background colour string to a QColor.
*/
QColor DBusHandler::convertBgColour(const QString& bgColor)
{
    if (bgColor.isEmpty())
        return Preferences::defaultBgColour();
    const QColor bg(bgColor);
    if (!bg.isValid())
        qCCritical(KALARM_LOG) << "D-Bus call: invalid background color:" << bgColor;
    return bg;
}

bool DBusHandler::convertRecurrence(KADateTime& start, KARecurrence& recurrence,
                                    const QString& startDateTime, const QString& icalRecurrence,
                    int subRepeatInterval, KCalendarCore::Duration& subRepeatDuration)
{
    start = convertDateTime(startDateTime);
    if (!start.isValid())
        return false;
    if (!recurrence.set(icalRecurrence))
        return false;
    if (subRepeatInterval  &&  recurrence.type() == KARecurrence::NO_RECUR)
    {
        subRepeatInterval = 0;
        qCWarning(KALARM_LOG) << "D-Bus call: no recurrence specified, so sub-repetition ignored";
    }
    if (subRepeatInterval  &&  !(subRepeatInterval % (24*60)))
        subRepeatDuration = Duration(subRepeatInterval / (24*60), Duration::Days);
    else
        subRepeatDuration = Duration(subRepeatInterval * 60, Duration::Seconds);
    return true;
}

bool DBusHandler::convertRecurrence(KADateTime& start, KARecurrence& recurrence, const QString& startDateTime,
                                    int recurType, int recurInterval, int recurCount)
{
    start = convertDateTime(startDateTime);
    if (!start.isValid())
        return false;
    return convertRecurrence(recurrence, start, recurType, recurInterval, recurCount, KADateTime());
}

bool DBusHandler::convertRecurrence(KADateTime& start, KARecurrence& recurrence, const QString& startDateTime,
                                    int recurType, int recurInterval, const QString& endDateTime)
{
    start = convertDateTime(startDateTime);
    if (!start.isValid())
        return false;
    const KADateTime end = convertDateTime(endDateTime, start);
    if (end.isDateOnly()  &&  !start.isDateOnly())
    {
        qCCritical(KALARM_LOG) << "D-Bus call: alarm is date-only, but recurrence end is date/time";
        return false;
    }
    if (!end.isDateOnly()  &&  start.isDateOnly())
    {
        qCCritical(KALARM_LOG) << "D-Bus call: alarm is timed, but recurrence end is date-only";
        return false;
    }
    return convertRecurrence(recurrence, start, recurType, recurInterval, 0, end);
}

bool DBusHandler::convertRecurrence(KARecurrence& recurrence, const KADateTime& start, int recurType,
                                    int recurInterval, int recurCount, const KADateTime& end)
{
    KARecurrence::Type type;
    switch (recurType)
    {
        case MINUTELY:  type = KARecurrence::MINUTELY;  break;
        case DAILY:     type = KARecurrence::DAILY;  break;
        case WEEKLY:    type = KARecurrence::WEEKLY;  break;
        case MONTHLY:   type = KARecurrence::MONTHLY_DAY;  break;
        case YEARLY:    type = KARecurrence::ANNUAL_DATE;  break;
        default:
            qCCritical(KALARM_LOG) << "D-Bus call: invalid recurrence type:" << recurType;
            return false;
    }
    recurrence.set(type, recurInterval, recurCount, start, end);
    return true;
}

#include "moc_dbushandler.cpp"

// vim: et sw=4:
