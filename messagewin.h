/*
 *  messagewin.h  -  displays an alarm message
 *  Program:  kalarm
 *  Copyright © 2001-2007 by David Jarvie <software@astrojar.org.uk>
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

#include <QList>

#include "mainwindowbase.h"
#include "alarmevent.h"

class QShowEvent;
class QMoveEvent;
class QResizeEvent;
class QCloseEvent;
class QPushButton;
class KPushButton;
class QLabel;
class QTimer;
class KWinModule;
class AlarmTimeWidget;
class DeferAlarmDlg;
namespace Phonon { class MediaObject; }

/**
 * MessageWin: A window to display an alarm message
 */
class MessageWin : public MainWindowBase
{
		Q_OBJECT
	public:
		enum {                // flags for constructor
			NO_RESCHEDULE = 0x01,    // don't reschedule the event once it has displayed
			NO_DEFER      = 0x02,    // don't display the Defer button
			NO_INIT_VIEW  = 0x04     // for internal MessageWin use only
		};

		MessageWin();     // for session management restoration only
		MessageWin(const KAEvent&, const KAAlarm&, int flags);
		MessageWin(const KAEvent&, const DateTime& alarmDateTime, const QStringList& errmsgs);
		~MessageWin();
		void                repeat(const KAAlarm&);
		void                setRecreating()        { mRecreating = true; }
		const DateTime&     dateTime()             { return mDateTime; }
		KAAlarm::Type       alarmType() const      { return mAlarmType; }
		bool                hasDefer() const       { return !!mDeferButton; }
		bool                isValid() const        { return !mInvalid; }
		virtual void        show();
		virtual QSize       sizeHint() const;
		static int          instanceCount()        { return mWindowList.count(); }
		static MessageWin*  findEvent(const QString& eventID);
		static void         redisplayAlarms();

	protected:
		virtual void        showEvent(QShowEvent*);
		virtual void        moveEvent(QMoveEvent*);
		virtual void        resizeEvent(QResizeEvent*);
		virtual void        closeEvent(QCloseEvent*);
		virtual void        saveProperties(KConfigGroup&);
		virtual void        readProperties(const KConfigGroup&);

	private slots:
		void                slotEdit();
		void                slotDefer();
		void                checkDeferralLimit();
		void                displayMainWindow();
		void                slotShowKMailMessage();
		void                slotSpeak();
		void                slotPlayAudio();
		void                checkAudioPlay();
		void                stopPlay();
		void                enableButtons();
		void                setRemainingTextDay();
		void                setRemainingTextMinute();
		void                setMaxSize();

	private:
		void                initView();
		void                displayComplete();
		void                playAudio();
		void                setDeferralLimit(const KAEvent&);
		void                alarmShowing(KAEvent&, const KCal::Event* = 0);
		bool                retrieveEvent(KAEvent&, AlarmResource*&, bool& showEdit, bool& showDefer);
		static bool         reinstateFromDisplaying(const KCal::Event*, KAEvent&, AlarmResource*&, bool& showEdit, bool& showDefer);

		static QList<MessageWin*> mWindowList;  // list of existing message windows
		// Properties needed by readProperties()
		QString             mMessage;
		QFont               mFont;
		QColor              mBgColour, mFgColour;
		DateTime            mDateTime;        // date/time displayed in the message window
		QDateTime           mCloseTime;       // local time at which window should be auto-closed
		QString             mEventID;
		QString             mAudioFile;
		float               mVolume;
		float               mFadeVolume;
		int                 mFadeSeconds;
		int                 mDefaultDeferMinutes;
		KAAlarm::Type       mAlarmType;
		KAEvent::Action     mAction;
		unsigned long       mKMailSerialNumber; // if email text, message's KMail serial number, else 0
		QStringList         mErrorMsgs;
		int                 mRestoreHeight;
		bool                mAudioRepeat;
		bool                mConfirmAck;
		bool                mShowEdit;        // display the Edit button
		bool                mNoDefer;         // don't display a Defer option
		bool                mInvalid;         // restored window is invalid
		// Sound file playing
		Phonon::MediaObject* mAudioObject;
		bool                mPlayedOnce;      // the sound file has started playing at least once
		// Miscellaneous
		KAEvent             mEvent;           // the whole event, for updating the calendar file
		AlarmResource*      mResource;        // resource which the event comes/came from
		QLabel*             mRemainingText;   // the remaining time (for a reminder window)
		KPushButton*        mOkButton;
		QPushButton*        mEditButton;
		QPushButton*        mDeferButton;
		QPushButton*        mSilenceButton;
		QPushButton*        mKAlarmButton;
		QPushButton*        mKMailButton;
		DeferAlarmDlg*      mDeferDlg;
		QDateTime           mDeferLimit;      // last time to which the message can currently be deferred
		mutable KWinModule* mWinModule;
		int                 mFlags;
		int                 mLateCancel;
		int                 mButtonDelay;     // delay (ms) after window is shown before buttons are enabled
		bool                mErrorWindow;     // the window is simply an error message
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
