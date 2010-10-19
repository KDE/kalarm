/*
 *  kaeventformatter.cpp  -  converts KAEvent properties to text
 *  Program:  kalarm
 *  Copyright © 2010 by David Jarvie <djarvie@kde.org>
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

#include "kaeventformatter.h"
#include "kaevent.h"
#include "datetime.h"

#include <kcalutils/incidenceformatter.h>

#include <kglobal.h>
#include <klocale.h>
#include <kdatetime.h>
#include <kdebug.h>

static QString trueFalse(bool value);
static QString number(unsigned long n);
static QString minutes(int n);
static QString dateTime(const KDateTime&);

KAEventFormatter::KAEventFormatter(const KAEvent& e, bool falseForUnspecified)
    : mEvent(e)
{
    if (falseForUnspecified)
        mUnspecifiedValue = trueFalse(false);
}

QString KAEventFormatter::label(Parameter param)
{
    switch (param)
    {
        case Id:                return i18nc("@label", "ID");
        case AlarmType:         return i18nc("@label", "Alarm type");
        case AlarmCategory:     return i18nc("@label", "Alarm status");
        case TemplateName:      return i18nc("@label", "Template name");
        case CreatedTime:       return i18nc("@label", "Creation time");
        case StartTime:         return i18nc("@label", "Start time");
        case TemplateAfterTime: return i18nc("@label", "Template after time");
        case Recurs:            return i18nc("@label", "Recurs");
        case Recurrence:        return i18nc("@label", "Recurrence");
        case RepeatInterval:    return i18nc("@label", "Sub repetition interval");
        case RepeatCount:       return i18nc("@label", "Sub repetition count");
        case WorkTimeOnly:      return i18nc("@label", "Work time only");
        case HolidaysExcluded:  return i18nc("@label", "Holidays excluded");
        case NextRecurrence:    return i18nc("@label", "Next recurrence");
        case Reminder:          return i18nc("@label", "Reminder");
        case DeferralTime:      return i18nc("@label", "Deferral");
        case DeferDefault:      return i18nc("@label", "Deferral default");
        case DeferDefaultDate:  return i18nc("@label", "Deferral default date only");
        case LateCancel:        return i18nc("@label", "Late cancel");
        case AutoClose:         return i18nc("@label", "Auto close");
        case CopyKOrganizer:    return i18nc("@label", "Copy to KOrganizer");
        case Enabled:           return i18nc("@label", "Enabled");
        case Archive:           return i18nc("@label", "Archive");
        case Revision:          return i18nc("@label", "Revision");

        case MessageText:       return i18nc("@label", "Message text");
        case MessageFile:       return i18nc("@label", "Message file");
        case FgColour:          return i18nc("@label", "Foreground color");
        case BgColour:          return i18nc("@label", "Background color");
        case Font:              return i18nc("@label", "Font");
        case PreAction:         return i18nc("@label", "Pre-alarm action");
        case PreActionCancel:   return i18nc("@label", "Pre-alarm action cancel");
        case PreActionNoError:  return i18nc("@label", "Pre-alarm action no error");
        case PostAction:        return i18nc("@label", "Post-alarm action");
        case ConfirmAck:        return i18nc("@label", "Confirm acknowledgement");
        case KMailSerial:       return i18nc("@label", "KMail serial number");
        case Sound:             return i18nc("@label Audio method", "Sound");
        case SoundRepeat:       return i18nc("@label", "Sound repeat");
        case SoundVolume:       return i18nc("@label", "Sound volume");
        case SoundFadeVolume:   return i18nc("@label", "Sound fade volume");
        case SoundFadeTime:     return i18nc("@label", "Sound fade time");

        case Command:           return i18nc("@label A shell command", "Command");
        case LogFile:           return i18nc("@label", "Log file");
        case CommandXTerm:      return i18nc("@label", "Command X-terminal");

        case EmailSubject:      return i18nc("@label", "Email subject");
        case EmailFromId:       return i18nc("@label Email address", "Email sender ID");
        case EmailTo:           return i18nc("@label Email address", "Email to");
        case EmailBcc:          return i18nc("@label true/false", "Email bcc");
        case EmailBody:         return i18nc("@label", "Email body");
        case EmailAttachments:  return i18nc("@label", "Email attachments");
    }
    return QString();
}

bool KAEventFormatter::isApplicable(Parameter param) const
{
    switch (param)
    {
        case Id:
        case AlarmType:
        case AlarmCategory:
        case CreatedTime:
        case StartTime:
        case Recurs:
        case Reminder:
        case LateCancel:
        case Enabled:
        case Archive:
        case Revision:
        case CopyKOrganizer:
            return true;
        case TemplateName:
        case TemplateAfterTime:
            return mEvent.isTemplate();
        case Recurrence:
        case RepeatCount:
        case WorkTimeOnly:
        case HolidaysExcluded:
        case NextRecurrence:
            return mEvent.recurs();
        case RepeatInterval:
            return mEvent.repetition();
        case AutoClose:
            return mEvent.lateCancel();
              

        case MessageText:
            return mEvent.action() == KAEvent::MESSAGE;
        case MessageFile:
            return mEvent.action() == KAEvent::FILE;
        case DeferralTime:
        case DeferDefault:
        case DeferDefaultDate:
        case FgColour:
        case BgColour:
        case Font:
        case PreAction:
        case PostAction:
        case ConfirmAck:
        case KMailSerial:
            return mEvent.displayAction();
        case PreActionCancel:
        case PreActionNoError:
            return !mEvent.preAction().isEmpty();
        case Sound:
            return mEvent.action() == KAEvent::MESSAGE  ||  mEvent.action() == KAEvent::AUDIO;
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
            return mEvent.action() == KAEvent::COMMAND;

        case EmailSubject:
        case EmailFromId:
        case EmailTo:
        case EmailBcc:
        case EmailBody:
        case EmailAttachments:
            return mEvent.action() == KAEvent::EMAIL;
    }
    return false;
}

QString KAEventFormatter::value(Parameter param) const
{
    switch (param)
    {
        case Id:                return mEvent.id();
        case AlarmType:
            switch (mEvent.action())
            {
                case KAEvent::MESSAGE:  return i18nc("@info/plain Alarm type", "Text display");
                case KAEvent::FILE:     return i18nc("@info/plain Alarm type", "File display");
                case KAEvent::COMMAND:  return mEvent.commandDisplay()
                                             ? i18nc("@info/plain Alarm type", "Command display")
                                             : i18nc("@info/plain Alarm type", "Command");
                case KAEvent::EMAIL:    return i18nc("@info/plain Alarm type", "Email");
                case KAEvent::AUDIO:    return i18nc("@info/plain Alarm type", "Audio");
            }
            break;
        case AlarmCategory:
            switch (mEvent.category())
            {
                case KAlarm::CalEvent::ACTIVE:    return i18nc("@info/plain Alarm type", "Active");
                case KAlarm::CalEvent::ARCHIVED:  return i18nc("@info/plain Alarm type", "Archived");
                case KAlarm::CalEvent::TEMPLATE:  return i18nc("@info/plain Alarm type", "Template");
                default:
                    break;
            }
            break;
        case TemplateName:      return mEvent.templateName();
        case CreatedTime:       return mEvent.createdDateTime().toUtc().toString("%Y-%m-%d %H:%M:%SZ");
        case StartTime:         return dateTime(mEvent.startDateTime().kDateTime());
        case TemplateAfterTime: return (mEvent.templateAfterTime() >= 0) ? number(mEvent.templateAfterTime()) : mUnspecifiedValue;
        case Recurs:            return trueFalse(mEvent.recurs());
        case Recurrence:
        {
            if (mEvent.repeatAtLogin())
                return i18nc("@info/plain Repeat at login", "At login until %1", dateTime(mEvent.mainDateTime().kDateTime()));
            KCalCore::Event event;
            KCalCore::Event::Ptr eptr(&event);
            mEvent.updateKCalEvent(eptr, KAEvent::UID_SET);
            return KCalUtils::IncidenceFormatter::recurrenceString(eptr);
        }
        case NextRecurrence:    return dateTime(mEvent.mainDateTime().kDateTime());
        case RepeatInterval:    return mEvent.repetitionText(true);
        case RepeatCount:       return number(mEvent.repetition().count());
        case WorkTimeOnly:      return trueFalse(mEvent.workTimeOnly());
        case HolidaysExcluded:  return trueFalse(mEvent.holidaysExcluded());
        case Reminder:          return mEvent.reminder() ? minutes(mEvent.reminder()) : mUnspecifiedValue;
        case DeferralTime:      return dateTime(mEvent.deferDateTime().kDateTime());
        case DeferDefault:      return minutes(mEvent.deferDefaultMinutes());
        case DeferDefaultDate:  return trueFalse(mEvent.deferDefaultDateOnly());
        case LateCancel:        return mEvent.lateCancel() ? minutes(mEvent.lateCancel()) : mUnspecifiedValue;
        case AutoClose:         return trueFalse(mEvent.lateCancel() ? mEvent.autoClose() : false);
        case CopyKOrganizer:    return trueFalse(mEvent.copyToKOrganizer());
        case Enabled:           return trueFalse(mEvent.enabled());
        case Archive:           return trueFalse(mEvent.toBeArchived());
        case Revision:          return number(mEvent.revision());

        case MessageText:       return mEvent.cleanText();
        case MessageFile:       return mEvent.cleanText();
        case FgColour:          return mEvent.fgColour().name();
        case BgColour:          return mEvent.bgColour().name();
        case Font:              return mEvent.useDefaultFont() ? i18nc("@info/plain Using default font", "Default") : mEvent.font().toString();
        case PreActionCancel:   return trueFalse(mEvent.cancelOnPreActionError());
        case PreActionNoError:  return trueFalse(mEvent.dontShowPreActionError());
        case PreAction:         return mEvent.preAction();
        case PostAction:        return mEvent.postAction();
        case ConfirmAck:        return trueFalse(mEvent.confirmAck());
        case KMailSerial:       return number(mEvent.kmailSerialNumber());
        case Sound:             return !mEvent.audioFile().isEmpty() ? mEvent.audioFile()
                                     : mEvent.speak() ? i18nc("@info/plain", "Speak")
                                     : mEvent.beep() ? i18nc("@info/plain", "Beep") : mUnspecifiedValue;
        case SoundRepeat:       return trueFalse(mEvent.repeatSound());
        case SoundVolume:       return mEvent.soundVolume() >= 0
                                     ? i18nc("@info/plain Percentage", "%1%%", static_cast<int>(mEvent.soundVolume() * 100))
                                     : mUnspecifiedValue;
        case SoundFadeVolume:   return mEvent.fadeVolume() >= 0
                                     ? i18nc("@info/plain Percentage", "%1%%", static_cast<int>(mEvent.fadeVolume() * 100))
                                     : mUnspecifiedValue;
        case SoundFadeTime:     return mEvent.fadeSeconds()
                                     ? i18nc("@info/plain", "%s seconds", mEvent.fadeSeconds())
                                     : mUnspecifiedValue;

        case Command:           return mEvent.cleanText();
        case LogFile:           return mEvent.logFile();
        case CommandXTerm:      return trueFalse(mEvent.commandXterm());

        case EmailSubject:      return mEvent.emailSubject();
        case EmailFromId:       return number(mEvent.emailFromId());
        case EmailTo:           return mEvent.emailAddresses(", ");
        case EmailBcc:          return trueFalse(mEvent.emailBcc());
        case EmailBody:         return mEvent.emailMessage();
        case EmailAttachments:  return mEvent.emailAttachments(", ");
    }
    return i18nc("@info/plain Error indication", "error!");
}

QString trueFalse(bool value)
{
    return value ? i18nc("@info/plain General purpose status indication: true or false", "true")
                 : i18nc("@info/plain General purpose status indication: true or false", "false");
}

// Convert an integer to digits for the locale.
// Do not use for date/time or monetary numbers (which have their own digit sets).
QString number(unsigned long n)
{
    KLocale* locale = KGlobal::locale();
    return locale->convertDigits(QString::number(n), locale->digitSet());
}

QString minutes(int n)
{
    return i18ncp("@info/plain", "1 minute", "%1 minutes", n);
}

QString dateTime(const KDateTime& dt)
{
    if (dt.isDateOnly())
        return dt.toString("%Y-%m-%d %:Z");
    else
        return dt.toString("%Y-%m-%d %H:%M %:Z");
}

// vim: et sw=4:
