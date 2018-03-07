/*
 *  kaeventformatter.cpp  -  converts KAlarmCal::KAEvent properties to text
 *  Copyright © 2010,2011 by David Jarvie <djarvie@kde.org>
 *
 *  This library is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU Library General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  This library is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 *  License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to the
 *  Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */

#include "kaeventformatter.h"

#include "kacalendar.h"
#include "kaevent.h"
#include "datetime.h"

#include <kcalutils/incidenceformatter.h>

#include <kglobal.h>
#include <kdatetime.h>
#include <KLocale>

static QString trueFalse(bool value);
static QString number(unsigned long n);
static QString minutes(int n);
static QString minutesHoursDays(int minutes);
static QString dateTime(const KDateTime &);

KAEventFormatter::KAEventFormatter(const KAEvent &e, bool falseForUnspecified)
    : mEvent(e)
{
    if (falseForUnspecified) {
        mUnspecifiedValue = trueFalse(false);
    }
}

QString KAEventFormatter::label(Parameter param)
{
    switch (param) {
    case Id:
        return i18nc("@label Unique identifier", "UID");
    case AlarmType:
        return i18nc("@label", "Alarm type");
    case AlarmCategory:
        return i18nc("@label", "Alarm status");
    case TemplateName:
        return i18nc("@label", "Template name");
    case CreatedTime:
        return i18nc("@label", "Creation time");
    case StartTime:
        return i18nc("@label", "Start time");
    case TemplateAfterTime:
        return i18nc("@label Start delay configured in an alarm template", "Template after time");
    case Recurs:
        return i18nc("@label", "Recurs");
    case Recurrence:
        return i18nc("@label", "Recurrence");
    case SubRepetition:
        return i18nc("@label", "Sub-repetition");
    case RepeatInterval:
        return i18nc("@label", "Sub-repetition interval");
    case RepeatCount:
        return i18nc("@label", "Sub-repetition count");
    case NextRepetition:
        return i18nc("@label", "Next sub-repetition");
    case WorkTimeOnly:
        return i18nc("@label", "Work time only");
    case HolidaysExcluded:
        return i18nc("@label", "Holidays excluded");
    case NextRecurrence:
        return i18nc("@label", "Next recurrence");
    case LateCancel:
        return i18nc("@label", "Late cancel");
    case AutoClose:
        return i18nc("@label Automatically close window", "Auto close");
    case CopyKOrganizer:
        return i18nc("@label", "Copy to KOrganizer");
    case Enabled:
        return i18nc("@label", "Enabled");
    case ReadOnly:
        return i18nc("@label", "Read-only");
    case Archive:
        return i18nc("@label Whether alarm should be archived", "Archive");
    case Revision:
        return i18nc("@label", "Revision");
    case CustomProperties:
        return i18nc("@label", "Custom properties");

    case MessageText:
        return i18nc("@label", "Message text");
    case MessageFile:
        return i18nc("@label File to provide text for message", "Message file");
    case FgColour:
        return i18nc("@label", "Foreground color");
    case BgColour:
        return i18nc("@label", "Background color");
    case Font:
        return i18nc("@label", "Font");
    case PreAction:
        return i18nc("@label Shell command to execute before alarm", "Pre-alarm action");
    case PreActionCancel:
        return i18nc("@label", "Pre-alarm action cancel");
    case PreActionNoError:
        return i18nc("@label", "Pre-alarm action no error");
    case PostAction:
        return i18nc("@label Shell command to execute after alarm", "Post-alarm action");
    case ConfirmAck:
        return i18nc("@label", "Confirm acknowledgement");
    case KMailSerial:
        return i18nc("@label", "KMail serial number");
    case Sound:
        return i18nc("@label Audio method", "Sound");
    case SoundRepeat:
        return i18nc("@label Whether audio should repeat", "Sound repeat");
    case SoundVolume:
        return i18nc("@label", "Sound volume");
    case SoundFadeVolume:
        return i18nc("@label", "Sound fade volume");
    case SoundFadeTime:
        return i18nc("@label", "Sound fade time");
    case Reminder:
        return i18nc("@label Whether the alarm has a reminder", "Reminder");
    case ReminderOnce:
        return i18nc("@label Whether reminder is on first recurrence only", "Reminder once only");
    case DeferralType:
        return i18nc("@label Deferral type", "Deferral");
    case DeferralTime:
        return i18nc("@label", "Deferral time");
    case DeferDefault:
        return i18nc("@label Default deferral delay", "Deferral default");
    case DeferDefaultDate:
        return i18nc("@label Whether deferral time is date-only by default", "Deferral default date only");

    case Command:
        return i18nc("@label A shell command", "Command");
    case LogFile:
        return i18nc("@label", "Log file");
    case CommandXTerm:
        return i18nc("@label Execute in terminal window", "Execute in terminal");

    case EmailSubject:
        return i18nc("@label", "Email subject");
    case EmailFromId:
        return i18nc("@label Email address", "Email sender ID");
    case EmailTo:
        return i18nc("@label Email address", "Email to");
    case EmailBcc:
        return i18nc("@label true/false", "Email bcc");
    case EmailBody:
        return i18nc("@label", "Email body");
    case EmailAttachments:
        return i18nc("@label", "Email attachments");
    }
    return QString();
}

bool KAEventFormatter::isApplicable(Parameter param) const
{
    switch (param) {
    case Id:
    case AlarmType:
    case AlarmCategory:
    case CreatedTime:
    case StartTime:
    case Recurs:
    case LateCancel:
    case Enabled:
    case ReadOnly:
    case Archive:
    case Revision:
    case CustomProperties:
    case CopyKOrganizer:
        return true;
    case TemplateName:
    case TemplateAfterTime:
        return mEvent.isTemplate();
    case Recurrence:
    case RepeatCount:
    case SubRepetition:
    case WorkTimeOnly:
    case HolidaysExcluded:
    case NextRecurrence:
        return mEvent.recurs();
    case RepeatInterval:
    case NextRepetition:
        return mEvent.repetition();
    case AutoClose:
        return mEvent.lateCancel();

    case MessageText:
        return mEvent.actionSubType() == KAEvent::MESSAGE;
    case MessageFile:
        return mEvent.actionSubType() == KAEvent::FILE;
    case FgColour:
    case BgColour:
    case Font:
    case PreAction:
    case PostAction:
    case ConfirmAck:
    case KMailSerial:
    case Reminder:
    case DeferralType:
    case DeferDefault:
        return mEvent.actionTypes() & KAEvent::ACT_DISPLAY;
    case ReminderOnce:
        return mEvent.reminderMinutes() && mEvent.recurs();
    case DeferralTime:
        return mEvent.deferred();
    case DeferDefaultDate:
        return mEvent.deferDefaultMinutes() > 0;
    case PreActionCancel:
    case PreActionNoError:
        return !mEvent.preAction().isEmpty();
    case Sound:
        return mEvent.actionSubType() == KAEvent::MESSAGE || mEvent.actionSubType() == KAEvent::AUDIO;
    case SoundRepeat:
        return !mEvent.audioFile().isEmpty();
    case SoundVolume:
        return mEvent.soundVolume() >= 0;
    case SoundFadeVolume:
    case SoundFadeTime:
        return mEvent.fadeVolume() >= 0;

    case Command:
    case LogFile:
    case CommandXTerm:
        return mEvent.actionSubType() == KAEvent::COMMAND;

    case EmailSubject:
    case EmailFromId:
    case EmailTo:
    case EmailBcc:
    case EmailBody:
    case EmailAttachments:
        return mEvent.actionSubType() == KAEvent::EMAIL;
    }
    return false;
}

QString KAEventFormatter::value(Parameter param) const
{
    switch (param) {
    case Id:
        return mEvent.id();
    case AlarmType:
        switch (mEvent.actionSubType()) {
        case KAEvent::MESSAGE:
            return i18nc("@info Alarm type", "Display (text)");
        case KAEvent::FILE:
            return i18nc("@info Alarm type", "Display (file)");
        case KAEvent::COMMAND:
            return mEvent.commandDisplay()
                   ? i18nc("@info Alarm type", "Display (command)")
                   : i18nc("@info Alarm type", "Command");
        case KAEvent::EMAIL:
            return i18nc("@info Alarm type", "Email");
        case KAEvent::AUDIO:
            return i18nc("@info Alarm type", "Audio");
        }
        break;
    case AlarmCategory:
        switch (mEvent.category()) {
        case CalEvent::ACTIVE:
            return i18nc("@info Alarm type", "Active");
        case CalEvent::ARCHIVED:
            return i18nc("@info Alarm type", "Archived");
        case CalEvent::TEMPLATE:
            return i18nc("@info Alarm type", "Template");
        default:
            break;
        }
        break;
    case TemplateName:
        return mEvent.templateName();
    case CreatedTime:
        return mEvent.createdDateTime().toUtc().toString(QStringLiteral("%Y-%m-%d %H:%M:%SZ"));
    case StartTime:
        return dateTime(mEvent.startDateTime().kDateTime());
    case TemplateAfterTime:
        return (mEvent.templateAfterTime() >= 0) ? number(mEvent.templateAfterTime()) : trueFalse(false);
    case Recurs:
        return trueFalse(mEvent.recurs());
    case Recurrence:
    {
        if (mEvent.repeatAtLogin(true)) {
            return i18nc("@info Repeat at login", "At login until %1", dateTime(mEvent.mainDateTime().kDateTime()));
        }
        KCalCore::Event::Ptr eptr(new KCalCore::Event);
        mEvent.updateKCalEvent(eptr, KAEvent::UID_SET);
        return KCalUtils::IncidenceFormatter::recurrenceString(eptr);
    }
    case NextRecurrence:
        return dateTime(mEvent.mainDateTime().kDateTime());
    case SubRepetition:
        return trueFalse(mEvent.repetition());
    case RepeatInterval:
        return mEvent.repetitionText(true);
    case RepeatCount:
        return mEvent.repetition() ? number(mEvent.repetition().count()) : QString();
    case NextRepetition:
        return mEvent.repetition() ? number(mEvent.nextRepetition()) : QString();
    case WorkTimeOnly:
        return trueFalse(mEvent.workTimeOnly());
    case HolidaysExcluded:
        return trueFalse(mEvent.holidaysExcluded());
    case LateCancel:
        return mEvent.lateCancel() ? minutesHoursDays(mEvent.lateCancel()) : trueFalse(false);
    case AutoClose:
        return trueFalse(mEvent.lateCancel() ? mEvent.autoClose() : false);
    case CopyKOrganizer:
        return trueFalse(mEvent.copyToKOrganizer());
    case Enabled:
        return trueFalse(mEvent.enabled());
    case ReadOnly:
        return trueFalse(mEvent.isReadOnly());
    case Archive:
        return trueFalse(mEvent.toBeArchived());
    case Revision:
        return number(mEvent.revision());
    case CustomProperties:
    {
        if (mEvent.customProperties().isEmpty()) {
            return QString();
        }
        QString value;
        const auto customProperties = mEvent.customProperties();
        for (auto it = customProperties.cbegin(), end = customProperties.cend(); it != end; ++it) {
            value += QString::fromLatin1(it.key()) + QLatin1String(":") + it.value() + QLatin1String("<nl/>");
        }
        return i18nc("@info", "%1", value);
    }

    case MessageText:
        return (mEvent.actionSubType() == KAEvent::MESSAGE) ? mEvent.cleanText() : QString();
    case MessageFile:
        return (mEvent.actionSubType() == KAEvent::FILE) ? mEvent.cleanText() : QString();
    case FgColour:
        return mEvent.fgColour().name();
    case BgColour:
        return mEvent.bgColour().name();
    case Font:
        return mEvent.useDefaultFont() ? i18nc("@info Using default font", "Default") : mEvent.font().toString();
    case PreActionCancel:
        return trueFalse(mEvent.extraActionOptions() & KAEvent::CancelOnPreActError);
    case PreActionNoError:
        return trueFalse(mEvent.extraActionOptions() & KAEvent::CancelOnPreActError);
    case PreAction:
        return mEvent.preAction();
    case PostAction:
        return mEvent.postAction();
    case Reminder:
        return mEvent.reminderMinutes() ? minutesHoursDays(mEvent.reminderMinutes()) : trueFalse(false);
    case ReminderOnce:
        return trueFalse(mEvent.reminderOnceOnly());
    case DeferralType:
        return mEvent.reminderDeferral() ? i18nc("@info", "Reminder") : trueFalse(mEvent.deferred());
    case DeferralTime:
        return mEvent.deferred() ? dateTime(mEvent.deferDateTime().kDateTime()) : trueFalse(false);
    case DeferDefault:
        return (mEvent.deferDefaultMinutes() > 0) ? minutes(mEvent.deferDefaultMinutes()) : trueFalse(false);
    case DeferDefaultDate:
        return trueFalse(mEvent.deferDefaultDateOnly());
    case ConfirmAck:
        return trueFalse(mEvent.confirmAck());
    case KMailSerial:
        return mEvent.kmailSerialNumber() ? number(mEvent.kmailSerialNumber()) : trueFalse(false);
    case Sound:
        return !mEvent.audioFile().isEmpty() ? mEvent.audioFile()
               : mEvent.speak() ? i18nc("@info", "Speak")
               : mEvent.beep() ? i18nc("@info", "Beep") : trueFalse(false);
    case SoundRepeat:
        return trueFalse(mEvent.repeatSound());
    case SoundVolume:
        return mEvent.soundVolume() >= 0
               ? i18nc("@info Percentage", "%1%%", static_cast<int>(mEvent.soundVolume() * 100))
               : mUnspecifiedValue;
    case SoundFadeVolume:
        return mEvent.fadeVolume() >= 0
               ? i18nc("@info Percentage", "%1%%", static_cast<int>(mEvent.fadeVolume() * 100))
               : mUnspecifiedValue;
    case SoundFadeTime:
        return mEvent.fadeSeconds()
               ? i18ncp("@info", "1 Second", "%1 Seconds", mEvent.fadeSeconds())
               : mUnspecifiedValue;

    case Command:
        return (mEvent.actionSubType() == KAEvent::COMMAND) ? mEvent.cleanText() : QString();
    case LogFile:
        return mEvent.logFile();
    case CommandXTerm:
        return trueFalse(mEvent.commandXterm());

    case EmailSubject:
        return mEvent.emailSubject();
    case EmailFromId:
        return (mEvent.actionSubType() == KAEvent::EMAIL) ? number(mEvent.emailFromId()) : QString();
    case EmailTo:
        return mEvent.emailAddresses(QStringLiteral(", "));
    case EmailBcc:
        return trueFalse(mEvent.emailBcc());
    case EmailBody:
        return mEvent.emailMessage();
    case EmailAttachments:
        return mEvent.emailAttachments(QStringLiteral(", "));
    }
    return i18nc("@info Error indication", "error!");
}

QString trueFalse(bool value)
{
    return value ? i18nc("@info General purpose status indication: yes or no", "Yes")
           : i18nc("@info General purpose status indication: yes or no", "No");
}

// Convert an integer to digits for the locale.
// Do not use for date/time or monetary numbers (which have their own digit sets).
QString number(unsigned long n)
{
    KLocale *locale = KLocale::global();
    return locale->convertDigits(QString::number(n), locale->digitSet());
}

QString minutes(int n)
{
    return i18ncp("@info", "1 Minute", "%1 Minutes", n);
}

QString dateTime(const KDateTime &dt)
{
    if (dt.isDateOnly()) {
        return dt.toString(QStringLiteral("%Y-%m-%d %:Z"));
    } else {
        return dt.toString(QStringLiteral("%Y-%m-%d %H:%M %:Z"));
    }
}

QString minutesHoursDays(int minutes)
{
    if (minutes % 60) {
        return i18ncp("@info", "1 Minute", "%1 Minutes", minutes);
    } else if (minutes % 1440) {
        return i18ncp("@info", "1 Hour", "%1 Hours", minutes / 60);
    } else {
        return i18ncp("@info", "1 Day", "%1 Days", minutes / 1440);
    }
}
