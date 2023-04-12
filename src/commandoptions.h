/*
 *  commandoptions.h  -  extract command line options
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2022 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#pragma once

#include "editdlg.h"
#include "kalarmcalendar/kaevent.h"
#include "kalarmcalendar/karecurrence.h"
#include "kalarmcalendar/kadatetime.h"

#include <QColor>
#include <QStringList>
class QCommandLineParser;
class QCommandLineOption;

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
    QStringList setOptions(QCommandLineParser*, const QStringList& args);
    static CommandOptions* firstInstance()        { return mFirstInstance; }
    CommandOptions();
    CommandOptions(CommandOptions&) = delete;
    CommandOptions& operator=(CommandOptions&) = delete;
    void                parse();
    void                process();
    Command             command() const           { return mCommand; }
    QString             commandName() const;
    QString             eventId() const           { return mEventId; }
    QString             resourceId() const        { return mResourceId; }
    QString             name() const              { return mName; }
    EditAlarmDlg::Type  editType() const          { return mEditType; }
    KAEvent::SubAction  editAction() const        { return mEditAction; }
    QString             text() const              { return mText; }
    KADateTime          alarmTime() const         { return mAlarmTime; }
    KARecurrence*       recurrence() const        { return mRecurrence; }
    int                 subRepeatCount() const    { return mRepeatCount; }
    KCalendarCore::Duration  subRepeatInterval() const { return mRepeatInterval; }
    int                 lateCancel() const        { return mLateCancel; }
    QColor              bgColour() const          { return mBgColour; }
    QColor              fgColour() const          { return mFgColour; }
    int                 reminderMinutes() const   { return mReminderMinutes; }
    QString             audioFile() const         { return mAudioFile; }
    float               audioVolume() const       { return mAudioVolume; }
    KCalendarCore::Person::List addressees() const     { return mAddressees; }
    QStringList         attachments() const       { return mAttachments; }
    QString             subject() const           { return mSubject; }
    uint                fromID() const            { return mFromID; }
    KAEvent::Flags      flags() const             { return mFlags; }
    bool                disableAll() const        { return mDisableAll; }
    QString             outputText() const        { return mError; }
#ifndef NDEBUG
    KADateTime          simulationTime() const    { return mSimulationTime; }
#endif
    static void         printError(const QString& errmsg);

private:
    class Private;
    friend class Private;
    Private* const d;
    QString     arg(int n);

    static CommandOptions* mFirstInstance;       // the first instance
    QCommandLineParser* mParser {nullptr};
    QList<QCommandLineOption*> mOptions;         // all possible command line options
    QStringList         mNonExecArguments;       // arguments except for --exec or --exec-display parameters
    QStringList         mExecArguments;          // arguments for --exec or --exec-display
    QString             mError;                  // error message
    Command             mCommand {NONE};         // the selected command
    QString             mEventId;                // TRIGGER_EVENT, CANCEL_EVENT, EDIT: event ID
    QString             mResourceId;             // TRIGGER_EVENT, CANCEL_EVENT, EDIT: optional resource ID
    QString             mName;                   // NEW, EDIT_NEW_PRESET: alarm/template name
    EditAlarmDlg::Type  mEditType;               // NEW, EDIT_NEW_*: alarm edit type
    KAEvent::SubAction  mEditAction;             // NEW: alarm edit sub-type
    bool                mEditActionSet {false};  // NEW: mEditAction is valid
    QString             mText;                   // NEW: alarm text
    KADateTime          mAlarmTime;              // NEW: alarm time
    KARecurrence*       mRecurrence {nullptr};   // NEW: recurrence
    int                 mRepeatCount {0};        // NEW: sub-repetition count
    KCalendarCore::Duration mRepeatInterval {0}; // NEW: sub-repetition interval
    int                 mLateCancel {0};         // NEW: late-cancellation interval
    QColor              mBgColour;               // NEW: background colour
    QColor              mFgColour;               // NEW: foreground colour
    int                 mReminderMinutes {0};    // NEW: reminder period
    QString             mAudioFile;              // NEW: audio file path
    float               mAudioVolume {-1.0f};    // NEW: audio file volume
    KCalendarCore::Person::List mAddressees;     // NEW: email addressees
    QStringList         mAttachments;            // NEW: email attachment file names
    QString             mSubject;                // NEW: email subject
    uint                mFromID {0};             // NEW: email sender ID
    KAEvent::Flags      mFlags;                  // NEW: event flags
    bool                mDisableAll {false};     // disable all alarm monitoring
#ifndef NDEBUG
    KADateTime          mSimulationTime;         // system time to be simulated, or invalid if none
#endif
};

// vim: et sw=4:
