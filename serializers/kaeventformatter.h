/*
 *  kaeventformatter.h  -  converts KAlarmCal::KAEvent properties to text
 *  SPDX-FileCopyrightText: 2010-2019 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KAEVENTFORMATTER_H
#define KAEVENTFORMATTER_H

#include <kalarmcal/kaevent.h>

#include <QString>

using namespace KAlarmCal;

class KAEventFormatter
{
public:
    // KAEvent parameter identifiers.
    // Note that parameters stored in Akonadi attributes are not included.
    enum Parameter {
        Id,
        AlarmType,
        AlarmCategory,
        TemplateName,
        CreatedTime,
        StartTime,
        TemplateAfterTime,
        Recurs,             // does the event recur?
        Recurrence,
        NextRecurrence,     // next alarm time excluding repetitions, including reminder/deferral
        SubRepetition,      // is there a sub-repetition?
        RepeatInterval,
        RepeatCount,
        NextRepetition,     // next repetition count
        LateCancel,
        AutoClose,
        WorkTimeOnly,
        HolidaysExcluded,
        CopyKOrganizer,
        Enabled,
        ReadOnly,
        Archive,
        Revision,
        CustomProperties,

        MessageText,
        MessageFile,
        FgColour,
        BgColour,
        Font,
        PreAction,
        PreActionCancel,
        PreActionNoError,
        PostAction,
        ConfirmAck,
        AkonadiItem,
        Sound,
        SoundRepeat,
        SoundVolume,
        SoundFadeVolume,
        SoundFadeTime,
        Reminder,
        ReminderOnce,
        DeferralType,
        DeferralTime,
        DeferDefault,
        DeferDefaultDate,

        Command,
        LogFile,
        CommandXTerm,

        EmailSubject,
        EmailFromId,
        EmailTo,
        EmailBcc,
        EmailBody,
        EmailAttachments
    };

    KAEventFormatter()
    {
    }

    KAEventFormatter(const KAEvent &e, bool falseForUnspecified);
    bool isApplicable(Parameter) const;
    QString value(Parameter) const;
    const KAEvent &event() const
    {
        return mEvent;
    }

    static QString label(Parameter);

private:
    KAEvent mEvent;
    QString mUnspecifiedValue;
};

#endif // KAEVENTFORMATTER_H
