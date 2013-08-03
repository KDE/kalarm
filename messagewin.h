/*
 *  messagewin.h  -  displays an alarm message
 *  Program:  kalarm
 *  Copyright © 2001-2013 by David Jarvie <djarvie@kde.org>
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

#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

/** @file messagewin.h - displays an alarm message */

#include "autoqpointer.h"
#ifdef USE_AKONADI
#include "eventid.h"
#endif
#include "mainwindowbase.h"

#include <kalarmcal/kaevent.h>

#ifdef USE_AKONADI
#include <akonadi/collection.h>
#include <akonadi/item.h>
#endif

#include <QList>
#include <QMap>
#include <QPointer>

class QShowEvent;
class QMoveEvent;
class QResizeEvent;
class QCloseEvent;
class PushButton;
class MessageText;
class QCheckBox;
class QLabel;
class DeferAlarmDlg;
class EditAlarmDlg;
class ShellProcess;
class AudioThread;

using namespace KAlarmCal;

/**
 * MessageWin: A window to display an alarm or error message
 */
class MessageWin : public MainWindowBase
{
        Q_OBJECT
    public:
        enum {                // flags for constructor
            NO_RESCHEDULE = 0x01,    // don't reschedule the event once it has displayed
            NO_DEFER      = 0x02,    // don't display the Defer button
            ALWAYS_HIDE   = 0x04,    // never show the window (e.g. for audio-only alarms)
            NO_INIT_VIEW  = 0x08     // for internal MessageWin use only
        };

        MessageWin();     // for session management restoration only
        MessageWin(const KAEvent*, const KAAlarm&, int flags);
        ~MessageWin();
        void                repeat(const KAAlarm&);
        void                setRecreating()        { mRecreating = true; }
        const DateTime&     dateTime()         { return mDateTime; }
        KAAlarm::Type       alarmType() const      { return mAlarmType; }
        bool                hasDefer() const;
        void                showDefer();
        void                cancelReminder(const KAEvent&, const KAAlarm&);
        void                showDateTime(const KAEvent&, const KAAlarm&);
        bool                isValid() const        { return !mInvalid; }
        bool                alwaysHidden() const   { return mAlwaysHide; }
        virtual void        show();
        virtual QSize       sizeHint() const;
        static int          instanceCount(bool excludeAlwaysHidden = false);
#ifdef USE_AKONADI
        static MessageWin*  findEvent(const EventId& eventId);
#else
        static MessageWin*  findEvent(const QString& eventId);
#endif
        static void         redisplayAlarms();
        static void         stopAudio(bool wait = false);
        static bool         isAudioPlaying();
        static void         showError(const KAEvent&, const DateTime& alarmDateTime, const QStringList& errmsgs,
                                      const QString& dontShowAgain = QString());
        static bool         spread(bool scatter);

    protected:
        virtual void        showEvent(QShowEvent*);
        virtual void        moveEvent(QMoveEvent*);
        virtual void        resizeEvent(QResizeEvent*);
        virtual void        closeEvent(QCloseEvent*);
        virtual void        saveProperties(KConfigGroup&);
        virtual void        readProperties(const KConfigGroup&);

    private slots:
        void                slotOk();
        void                slotEdit();
        void                slotDefer();
        void                editCloseOk();
        void                editCloseCancel();
        void                activeWindowChanged(WId);
        void                checkDeferralLimit();
        void                displayMainWindow();
#ifdef KMAIL_SUPPORTED
        void                slotShowKMailMessage();
#endif
        void                slotSpeak();
        void                audioTerminating();
        void                startAudio();
        void                playReady();
        void                playFinished();
        void                enableButtons();
        void                setRemainingTextDay();
        void                setRemainingTextMinute();
        void                frameDrawn();
        void                readProcessOutput(ShellProcess*);

    private:
        MessageWin(const KAEvent*, const DateTime& alarmDateTime, const QStringList& errmsgs,
                   const QString& dontShowAgain);
        void                initView();
        QString             dateTimeToDisplay();
        void                displayComplete();
        void                setButtonsReadOnly(bool);
        bool                getWorkAreaAndModal();
        void                playAudio();
        void                setDeferralLimit(const KAEvent&);
        void                alarmShowing(KAEvent&);
#ifdef USE_AKONADI
        bool                retrieveEvent(KAEvent&, Akonadi::Collection&, bool& showEdit, bool& showDefer);
#else
        bool                retrieveEvent(KAEvent&, AlarmResource*&, bool& showEdit, bool& showDefer);
#endif
        bool                haveErrorMessage(unsigned msg) const;
        void                clearErrorMessage(unsigned msg) const;
#ifdef USE_AKONADI
        static bool         reinstateFromDisplaying(const KCalCore::Event::Ptr&, KAEvent&, Akonadi::Collection&, bool& showEdit, bool& showDefer);
#else
        static bool         reinstateFromDisplaying(const KCal::Event*, KAEvent&, AlarmResource*&, bool& showEdit, bool& showDefer);
#endif
        static bool         isSpread(const QPoint& topLeft);

        static QList<MessageWin*>      mWindowList;    // list of existing message windows
#ifdef USE_AKONADI
        static QMap<EventId, unsigned> mErrorMessages; // error messages currently displayed, by event ID
#else
        static QMap<QString, unsigned> mErrorMessages; // error messages currently displayed, by event ID
#endif
        // Sound file playing
        static QPointer<AudioThread> mAudioThread;   // thread to play audio file
        // Properties needed by readProperties()
        QString             mMessage;
        QFont               mFont;
        QColor              mBgColour, mFgColour;
        DateTime            mDateTime;        // date/time displayed in the message window
        QDateTime           mCloseTime;       // UTC time at which window should be auto-closed
#ifdef USE_AKONADI
        Akonadi::Item::Id   mEventItemId;
        EventId             mEventId;
#else
        QString             mEventId;
#endif
        QString             mAudioFile;
        float               mVolume;
        float               mFadeVolume;
        int                 mFadeSeconds;
        int                 mDefaultDeferMinutes;
        KAAlarm::Type       mAlarmType;
        KAEvent::SubAction  mAction;
        unsigned long       mKMailSerialNumber; // if email text, message's KMail serial number, else 0
        KAEvent::CmdErrType mCommandError;
        QStringList         mErrorMsgs;
        QString             mDontShowAgain;   // non-null for don't-show-again option with error message
        int                 mRestoreHeight;
        int                 mAudioRepeatPause;
        bool                mConfirmAck;
        bool                mShowEdit;        // display the Edit button
        bool                mNoDefer;         // don't display a Defer option
        bool                mInvalid;         // restored window is invalid
        // Miscellaneous
        KAEvent             mEvent;           // the whole event, for updating the calendar file
        KAEvent             mOriginalEvent;   // the original event supplied to the constructor
#ifdef USE_AKONADI
        Akonadi::Collection mCollection;      // collection which the event comes/came from
#else
        AlarmResource*      mResource;        // resource which the event comes/came from
#endif
        QLabel*             mTimeLabel;       // trigger time label
        QLabel*             mRemainingText;   // the remaining time (for a reminder window)
        PushButton*         mOkButton;
        PushButton*         mEditButton;
        PushButton*         mDeferButton;
        PushButton*         mSilenceButton;
        PushButton*         mKAlarmButton;
        PushButton*         mKMailButton;
        MessageText*        mCommandText;     // shows output from command
        QCheckBox*          mDontShowAgainCheck;
        EditAlarmDlg*       mEditDlg;         // alarm edit dialog invoked by Edit button
        DeferAlarmDlg*      mDeferDlg;
        QDateTime           mDeferLimit;      // last UTC time to which the message can currently be deferred
        int                 mButtonDelay;     // delay (ms) after window is shown before buttons are enabled
        int                 mScreenNumber;    // screen to display on, or -1 for default
        bool                mAlwaysHide;      // the window should never be displayed
        bool                mErrorWindow;     // the window is simply an error message
        bool                mInitialised;     // initView() has been called to create the window's widgets
        bool                mNoPostAction;    // don't execute any post-alarm action
        bool                mRecreating;      // window is about to be deleted and immediately recreated
        bool                mBeep;
        bool                mSpeak;           // the message should be spoken via kttsd
        bool                mRescheduleEvent; // true to delete event after message has been displayed
        bool                mShown;           // true once the window has been displayed
        bool                mPositioning;     // true when the window is being positioned initially
        bool                mNoCloseConfirm;  // the Defer or Edit button is closing the dialog
        bool                mDisableDeferral; // true if past deferral limit, so don't enable Defer button
};

#endif // MESSAGEWIN_H

// vim: et sw=4:
