/*
 *  kaeventformatter.cpp  -  converts KAlarmCal::KAEvent properties to text
 *  SPDX-FileCopyrightText: 2010-2023 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kaeventformatter.h"

#include "kacalendar.h"
#include "kaevent.h"
#include "datetime.h"

#include <KCalUtils/IncidenceFormatter>

#include <KLocalizedString>

#include <QLocale>
using namespace Qt::Literals::StringLiterals;

static QString trueFalse(bool value);
static QString minutes(int n);
static QString minutesHoursDays(int minutes);
static QString dateTime(const KAlarmCal::KADateTime&);

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
        case Id:
            return i18nc("@label Unique identifier", "UID");
        case AlarmType:
            return i18nc("@label", "Alarm type");
        case AlarmCategory:
            return i18nc("@label", "Alarm status");
        case Name:
            return i18nc("@label", "Alarm name");
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
        case AkonadiItem:
            return i18nc("@label", "Akonadi Item ID");
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
        case Notify:
            return i18nc("@label Whether to use standard notification system", "Notify");
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
        case CommandHideError:
            return i18nc("@label", "Hide command error");

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
    return {};
}

bool KAEventFormatter::isApplicable(Parameter param) const
{
    switch (param)
    {
        case Id:
        case AlarmType:
        case AlarmCategory:
        case Name:
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
            return mEvent.actionSubType() == KAEvent::SubAction::Message;
        case MessageFile:
            return mEvent.actionSubType() == KAEvent::SubAction::File;
        case FgColour:
        case BgColour:
        case Font:
        case PreAction:
        case PostAction:
        case ConfirmAck:
        case AkonadiItem:
        case Reminder:
        case Notify:
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
            return mEvent.actionSubType() == KAEvent::SubAction::Message || mEvent.actionSubType() == KAEvent::SubAction::Audio;
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
        case CommandHideError:
            return mEvent.actionSubType() == KAEvent::SubAction::Command;

        case EmailSubject:
        case EmailFromId:
        case EmailTo:
        case EmailBcc:
        case EmailBody:
        case EmailAttachments:
            return mEvent.actionSubType() == KAEvent::SubAction::Email;
    }
    return false;
}

QString KAEventFormatter::value(Parameter param) const
{
    switch (param)
    {
        case Id:
            return mEvent.id();
        case AlarmType:
            switch (mEvent.actionSubType())
            {
                case KAEvent::SubAction::Message:
                    return i18nc("@info Alarm type", "Display (text)");
                case KAEvent::SubAction::File:
                    return i18nc("@info Alarm type", "Display (file)");
                case KAEvent::SubAction::Command:
                    return mEvent.commandDisplay()
                           ? i18nc("@info Alarm type", "Display (command)")
                           : i18nc("@info Alarm type", "Command");
                case KAEvent::SubAction::Email:
                    return i18nc("@info Alarm type", "Email");
                case KAEvent::SubAction::Audio:
                    return i18nc("@info Alarm type", "Audio");
            }
            break;
        case AlarmCategory:
            switch (mEvent.category())
            {
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
        case Name:
        case TemplateName:
            return mEvent.name();
        case CreatedTime:
            return mEvent.createdDateTime().toUtc().toString(QStringLiteral("%Y-%m-%d %H:%M:%SZ"));
        case StartTime:
            return dateTime(mEvent.startDateTime().kDateTime());
        case TemplateAfterTime:
            return (mEvent.templateAfterTime() >= 0) ? QLocale().toString(mEvent.templateAfterTime()) : trueFalse(false);
        case Recurs:
            return trueFalse(mEvent.recurs());
        case Recurrence:
        {
            if (mEvent.repeatAtLogin(true))
                return i18nc("@info Repeat at login", "At login until %1", dateTime(mEvent.mainDateTime().kDateTime()));
            KCalendarCore::Event::Ptr eptr(new KCalendarCore::Event);
            mEvent.updateKCalEvent(eptr, KAEvent::UidAction::Set);
            return KCalUtils::IncidenceFormatter::recurrenceString(eptr);
        }
        case NextRecurrence:
            return dateTime(mEvent.mainDateTime().kDateTime());
        case SubRepetition:
            return trueFalse(mEvent.repetition());
        case RepeatInterval:
            return mEvent.repetitionText(true);
        case RepeatCount:
            return mEvent.repetition() ? QLocale().toString(mEvent.repetition().count()) : QString();
        case NextRepetition:
            return mEvent.repetition() ? QLocale().toString(mEvent.nextRepetition()) : QString();
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
            return QLocale().toString(mEvent.revision());
        case CustomProperties:
        {
            if (mEvent.customProperties().isEmpty())
                return {};
            QString value;
            const auto customProperties = mEvent.customProperties();
            for (auto it = customProperties.cbegin(), end = customProperties.cend(); it != end; ++it)
                value += QString::fromLatin1(it.key()) + ":"_L1 + it.value() + "<nl/>"_L1;
            return i18nc("@info", "%1", value);
        }

        case MessageText:
            return (mEvent.actionSubType() == KAEvent::SubAction::Message) ? mEvent.cleanText() : QString();
        case MessageFile:
            return (mEvent.actionSubType() == KAEvent::SubAction::File) ? mEvent.cleanText() : QString();
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
        case Notify:
            return trueFalse(mEvent.notify());
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
        case AkonadiItem:
            return (mEvent.emailId() >= 0) ? QLocale().toString(mEvent.emailId()) : trueFalse(false);
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
            return (mEvent.actionSubType() == KAEvent::SubAction::Command) ? mEvent.cleanText() : QString();
        case LogFile:
            return mEvent.logFile();
        case CommandXTerm:
            return trueFalse(mEvent.commandXterm());
        case CommandHideError:
            return trueFalse(mEvent.commandHideError());

        case EmailSubject:
            return mEvent.emailSubject();
        case EmailFromId:
            return (mEvent.actionSubType() == KAEvent::SubAction::Email) ? QLocale().toString(mEvent.emailFromId()) : QString();
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

QString minutes(int n)
{
    return i18ncp("@info", "1 Minute", "%1 Minutes", n);
}

QString dateTime(const KAlarmCal::KADateTime& dt)
{
    if (dt.isDateOnly())
        return dt.toString(QStringLiteral("%Y-%m-%d %:Z"));
    else
        return dt.toString(QStringLiteral("%Y-%m-%d %H:%M %:Z"));
}

QString minutesHoursDays(int minutes)
{
    if (minutes % 60)
        return i18ncp("@info", "1 Minute", "%1 Minutes", minutes);
    else if (minutes % 1440)
        return i18ncp("@info", "1 Hour", "%1 Hours", minutes / 60);
    else
        return i18ncp("@info", "1 Day", "%1 Days", minutes / 1440);
}

// vim: et sw=4:
