/*
 *  kaeventformatter.h  -  converts KAEvent properties to text
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

#ifndef KAEVENTFORMATTER_H
#define KAEVENTFORMATTER_H

#include "kaevent.h"

#include <QString>

class KAEvent;

class KAEventFormatter
{
    public:
        // KAEvent parameter identifiers.
        // Note that parameters stored in Akonadi attributes are not included.
        enum Parameter
        {
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
            KMailSerial,
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

        KAEventFormatter() {}
        KAEventFormatter(const KAEvent& e, bool falseForUnspecified);
        bool           isApplicable(Parameter) const;
        QString        value(Parameter) const;
        const KAEvent& event() const   { return mEvent; }
        static QString label(Parameter);

    private:
        KAEvent mEvent;
        QString mUnspecifiedValue;
};

#endif // KAEVENTFORMATTER_H

// vim: et sw=4:
