/*
 *  commandoptions.h  -  extract command line options
 *  Program:  kalarm
 *  Copyright © 2001-2012 by David Jarvie <djarvie@kde.org>
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

#ifndef COMMANDOPTIONS_H
#define COMMANDOPTIONS_H

#include "editdlg.h"
#ifdef USE_AKONADI
#include "eventid.h"
#endif

#include <kalarmcal/kaevent.h>
#include <kalarmcal/karecurrence.h>

#include <kdatetime.h>
#include <QByteArray>
#include <QColor>
#include <QStringList>
class KCmdLineArgs;

using namespace KAlarmCal;

class CommandOptions
{
    public:
        enum Command
        {
            CMD_ERROR,        // error in command line options
            NONE,             // no command
            TRAY,             // --tray
            TRIGGER_EVENT,    // --triggerEvent
            CANCEL_EVENT,     // --cancelEvent
            EDIT,             // --edit
            EDIT_NEW_PRESET,  // --edit-new-preset
            EDIT_NEW,         // --edit-new-display, --edit-new-command, --edit-new-email
            NEW,              // --file, --exec-display, --exec, --mail, message
            LIST              // --list
        };
        CommandOptions();
        Command             command() const           { return mCommand; }
        QString             commandName() const       { return QString::fromLatin1(mCommandName); }
#ifdef USE_AKONADI
        EventId             eventId() const           { return mEventId; }
#else
        QString             eventId() const           { return mEventId; }
#endif
        QString             templateName() const      { return mTemplateName; }
        EditAlarmDlg::Type  editType() const          { return mEditType; }
        KAEvent::SubAction  editAction() const        { return mEditAction; }
        QString             text() const              { return mText; }
        KDateTime           alarmTime() const         { return mAlarmTime; }
        KARecurrence*       recurrence() const        { return mRecurrence; }
        int                 subRepeatCount() const    { return mRepeatCount; }
        int                 subRepeatInterval() const { return mRepeatInterval; }
        int                 lateCancel() const        { return mLateCancel; }
        QColor              bgColour() const          { return mBgColour; }
        QColor              fgColour() const          { return mFgColour; }
        int                 reminderMinutes() const   { return mReminderMinutes; }
        QString             audioFile() const         { return mAudioFile; }
        float               audioVolume() const       { return mAudioVolume; }
#ifdef USE_AKONADI
        KCalCore::Person::List addressees() const     { return mAddressees; }
#else
        QList<KCal::Person> addressees() const        { return mAddressees; }
#endif
        QStringList         attachments() const       { return mAttachments; }
        QString             subject() const           { return mSubject; }
        uint                fromID() const            { return mFromID; }
        KAEvent::Flags      flags() const             { return mFlags; }
        bool                disableAll() const        { return mDisableAll; }
#ifndef NDEBUG
        KDateTime           simulationTime() const    { return mSimulationTime; }
#endif
        static void         printError(const QString& errmsg);

    private:
        bool        checkCommand(const QByteArray& command, Command, EditAlarmDlg::Type = EditAlarmDlg::NO_TYPE);
        inline void setError(const QString& error);
        void        setErrorRequires(const char* opt, const char* opt2, const char* opt3 = 0);
        void        setErrorParameter(const char* opt);
        void        setErrorIncompatible(const QByteArray& opt1, const QByteArray& opt2);
        void        checkEditType(EditAlarmDlg::Type type, const QByteArray& opt)
                                  { checkEditType(type, EditAlarmDlg::NO_TYPE, opt); }
        void        checkEditType(EditAlarmDlg::Type, EditAlarmDlg::Type, const QByteArray& opt);

        KCmdLineArgs*       mArgs;
        QString             mError;          // error message
        Command             mCommand;        // the selected command
        QByteArray          mCommandName;    // option string for the selected command
#ifdef USE_AKONADI
        EventId             mEventId;        // TRIGGER_EVENT, CANCEL_EVENT, EDIT: event ID
#else
        QString             mEventId;        // TRIGGER_EVENT, CANCEL_EVENT, EDIT: event ID
#endif
        QString             mTemplateName;   // EDIT_NEW_PRESET: template name
        EditAlarmDlg::Type  mEditType;       // NEW, EDIT_NEW_*: alarm edit type
        KAEvent::SubAction  mEditAction;     // NEW: alarm edit sub-type
        bool                mEditActionSet;  // NEW: mEditAction is valid
        QString             mText;           // NEW: alarm text
        KDateTime           mAlarmTime;      // NEW: alarm time
        KARecurrence*       mRecurrence;     // NEW: recurrence
        int                 mRepeatCount;    // NEW: sub-repetition count
        int                 mRepeatInterval; // NEW: sub-repetition interval
        int                 mLateCancel;     // NEW: late-cancellation interval
        QColor              mBgColour;       // NEW: background colour
        QColor              mFgColour;       // NEW: foreground colour
        int                 mReminderMinutes;// NEW: reminder period
        QString             mAudioFile;      // NEW: audio file path
        float               mAudioVolume;    // NEW: audio file volume
#ifdef USE_AKONADI
        KCalCore::Person::List mAddressees;  // NEW: email addressees
#else
        QList<KCal::Person> mAddressees;     // NEW: email addressees
#endif
        QStringList         mAttachments;    // NEW: email attachment file names
        QString             mSubject;        // NEW: email subject
        uint                mFromID;         // NEW: email sender ID
        KAEvent::Flags      mFlags;          // NEW: event flags
        bool                mDisableAll;     // disable all alarm monitoring
#ifndef NDEBUG
        KDateTime           mSimulationTime; // system time to be simulated, or invalid if none
#endif
};

#endif // COMMANDOPTIONS_H

// vim: et sw=4:
