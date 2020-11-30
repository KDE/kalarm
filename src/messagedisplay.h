/*
 *  messagedisplay.h  -  base class to display an alarm or error message
 *  Program:  kalarm
 *  SPDX-FileCopyrightText: 2001-2020 David Jarvie <djarvie@kde.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MESSAGEDISPLAY_H
#define MESSAGEDISPLAY_H

#include "messagedisplayhelper.h"

#include <KAlarmCal/KAEvent>


class KConfigGroup;
class DeferAlarmDlg;
class EventId;
class MessageDisplayHelper;

using namespace KAlarmCal;

/**
 * Abstract base class for alarm message display.
 */
class MessageDisplay
{
public:
    /** Flags for constructor. */
    enum
    {
        NoReschedule     = 0x01,    // don't reschedule the event once it has displayed
        NoDefer          = 0x02,    // don't display an option to defer
        NoRecordCmdError = 0x04,    // don't record error executing command
        AlwaysHide       = 0x08,    // never show the window (e.g. for audio-only alarms)
        NoInitView       = 0x10     // for internal MessageDisplayHelper use only
    };

    /** Create a MessageDisplay alarm message instance.
     *  The instance type is dependent on the event->notify() value.
     */
    static MessageDisplay* create(const KAEvent& event, const KAAlarm& alarm, int flags);

    /** Show an error message about the execution of an alarm.
     *  The instance type is dependent on the event.notify() value.
     *  @param event          The event for the alarm.
     *  @param alarmDateTime  Date/time displayed in the message display.
     *  @param errmsgs        The error messages to display.
     *  @param dontShowAgain  The "don't show again" ID of the error message.
     */
    static void showError(const KAEvent& event, const DateTime& alarmDateTime,
                          const QStringList& errmsgs, const QString& dontShowAgain = QString());

    virtual ~MessageDisplay();

    bool isValid() const   { return mHelper->isValid(); }
    KAAlarm::Type alarmType() const   { return mHelper->mAlarmType; }

    /** Raise the alarm display, re-output any required audio notification, and
     *  reschedule the alarm in the calendar file.
     */
    virtual void repeat(const KAAlarm&) = 0;

    virtual bool hasDefer() const = 0;
    virtual void showDefer() = 0;
    virtual void showDateTime(const KAEvent&, const KAAlarm&)  {}

    /** Convert a reminder display into a normal alarm display. */
    virtual void cancelReminder(const KAEvent& event, const KAAlarm& alarm)
    {
        mHelper->cancelReminder(event, alarm);
    }

    /** Returns the existing message display (if any) which is showing the event with
     *  the specified ID.
     */
    static MessageDisplay* findEvent(const EventId& eventId, MessageDisplay* exclude = nullptr)
    { return MessageDisplayHelper::findEvent(eventId, exclude); }

    /** Redisplay alarms which were being shown when the program last exited. */
    static void redisplayAlarms();

    /** Retrieve the event with the current ID from the displaying calendar file,
     *  or if not found there, from the archive calendar.
     *  @param resource  Is set to the resource which originally contained the
     *                   event, or invalid if not known.
     */
    static bool retrieveEvent(const EventId&, KAEvent&, Resource&, bool& showEdit, bool& showDefer);

    void        playAudio()                    { mHelper->playAudio(); }
    static void stopAudio(bool wait = false)   { MessageDisplayHelper::stopAudio(wait); }
    static bool isAudioPlaying()               { return MessageDisplayHelper::isAudioPlaying(); }

    /** Called when the edit alarm dialog has been cancelled. */
    virtual void editDlgCancelled()  {}

    /** For use by MessageDisplayHelper.
     *  Returns the widget to act as parent for error messages, etc. */
    virtual QWidget* displayParent() = 0;

    /** For use by MessageDisplayHelper only. Close the alarm message display. */
    virtual void closeDisplay() = 0;

    /** Show the alarm message display. */
    virtual void showDisplay() = 0;

    /** For use by MessageDisplayHelper only. Raise the alarm message display. */
    virtual void raiseDisplay() = 0;

    static int instanceCount(bool excludeAlwaysHidden = false)    { return MessageDisplayHelper::instanceCount(excludeAlwaysHidden); }

protected:
    MessageDisplay();
    MessageDisplay(const KAEvent& event, const KAAlarm& alarm, int flags);
    MessageDisplay(const KAEvent& event, const DateTime& alarmDateTime,
                   const QStringList& errmsgs, const QString& dontShowAgain);

    explicit MessageDisplay(MessageDisplayHelper* helper);

    /** Called by MessageDisplayHelper to confirm that the alarm message should be
     *  acknowledged (closed).
     *  @return  true to close the alarm message, false to keep it open.
     */
    virtual bool confirmAcknowledgement()  { return true; }

    /** Set up the alarm message display. */
    virtual void setUpDisplay() = 0;

    void displayMainWindow();

    virtual bool isDeferButtonEnabled() const = 0;
    virtual void enableDeferButton(bool enable) = 0;
    virtual void enableEditButton(bool enable) = 0;

    // Holds data required by defer dialog.
    // This is needed because the display may have closed when the defer dialog
    // is opened.
    struct DeferDlgData
    {
        DeferAlarmDlg*      dlg;
        EventId             eventId;
        KAAlarm::Type       alarmType;
        KAEvent::CmdErrType commandError;
        bool                displayOpen;

        DeferDlgData(DeferAlarmDlg* d) : dlg(d) {}
        ~DeferDlgData();
    };

    DeferDlgData* createDeferDlg(bool displayClosing);
    void          executeDeferDlg(DeferDlgData* data);

    MessageDisplayHelper* mHelper;

    // Access to MessageDisplayHelper data.
    KAAlarm::Type&      mAlarmType()             { return mHelper->mAlarmType; }
    KAEvent::SubAction  mAction() const          { return mHelper->mAction; }
    const EventId&      mEventId() const         { return mHelper->mEventId; }
    const KAEvent&      mEvent() const           { return mHelper->mEvent; }
    const KAEvent&      mOriginalEvent() const   { return mHelper->mOriginalEvent; }
    const QFont&        mFont() const            { return mHelper->mFont; }
    const QColor&       mBgColour() const        { return mHelper->mBgColour; }
    const QColor&       mFgColour() const        { return mHelper->mFgColour; }
    const QString&      mAudioFile() const       { return mHelper->mAudioFile; }
    float               mVolume() const          { return mHelper->mVolume; }
    float               mFadeVolume() const      { return mHelper->mFadeVolume; }
    int                 mDefaultDeferMinutes() const  { return mHelper->mDefaultDeferMinutes; }
    Akonadi::Item::Id   mAkonadiItemId() const   { return mHelper->mAkonadiItemId; }
    KAEvent::CmdErrType mCommandError() const    { return mHelper->mCommandError; }
    bool&               mNoPostAction()          { return mHelper->mNoPostAction; }

    const DateTime&     mDateTime() const        { return mHelper->mDateTime; }
    EditAlarmDlg*       mEditDlg() const         { return mHelper->mEditDlg; }
    bool                mIsValid() const         { return !mHelper->mInvalid; }
    bool                mDisableDeferral() const { return mHelper->mDisableDeferral; }
    bool                mNoCloseConfirm() const  { return mHelper->mNoCloseConfirm; }
    bool                mAlwaysHidden() const    { return mHelper->mAlwaysHide; }
    bool                mErrorWindow() const     { return mHelper->mErrorWindow; }
    const QStringList&  mErrorMsgs() const       { return mHelper->mErrorMsgs; }
    bool&               mNoDefer()               { return mHelper->mNoDefer; }
    bool                mShowEdit() const        { return mHelper->mShowEdit; }
    const QString&      mDontShowAgain() const   { return mHelper->mDontShowAgain; }

private:
    static bool reinstateFromDisplaying(const KCalendarCore::Event::Ptr&, KAEvent&, Resource&, bool& showEdit, bool& showDefer);

    static bool     mRedisplayed;   // redisplayAlarms() was called

friend class MessageDisplayHelper;
};

#endif // MESSAGEDISPLAY_H

// vim: et sw=4:
