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

#include <qmap.h>

#include "mainwindowbase.h"
#include "alarmevent.h"

class QPushButton;
class KPushButton;
class QLabel;
class QTimer;
class KWinModule;
class AlarmTimeWidget;
class DeferAlarmDlg;
class KArtsDispatcher;
namespace KDE { class PlayObject; }

/**
 * MessageWin: A window to display an alarm message
 */
class MessageWin : public MainWindowBase
{
		Q_OBJECT
	public:
		MessageWin();     // for session management restoration only
		MessageWin(const KAEvent&, const KAAlarm&, bool reschedule_event = true, bool allowDefer = true);
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

	protected:
		virtual void        showEvent(QShowEvent*);
		virtual void        moveEvent(QMoveEvent*);
		virtual void        resizeEvent(QResizeEvent*);
		virtual void        closeEvent(QCloseEvent*);
		virtual void        saveProperties(KConfig*);
		virtual void        readProperties(KConfig*);

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
		void                slotFade();
		void                enableButtons();
		void                setRemainingTextDay();
		void                setRemainingTextMinute();
		void                setMaxSize();

	private:
		void                initView();
#ifndef WITHOUT_ARTS
		void                initAudio(bool firstTime);
		int                 getKMixVolume();
		void                setKMixVolume(int percent);
#endif
		void                displayComplete();
		void                playAudio();
		void                setDeferralLimit(const KAEvent&);
		bool                haveErrorMessage(unsigned msg) const;
		void                clearErrorMessage(unsigned msg) const;

		static QValueList<MessageWin*> mWindowList;  // list of existing message windows
		static QMap<QString, unsigned> mErrorMessages;  // error messages currently displayed, by event ID
		// Properties needed by readProperties()
		QString             mMessage;
		QFont               mFont;
		QColor              mBgColour, mFgColour;
		DateTime            mDateTime;        // date/time displayed in the message window
		QDateTime           mCloseTime;       // time at which window should be auto-closed
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
		KArtsDispatcher*    mArtsDispatcher;
		KDE::PlayObject*    mPlayObject;
		QCString            mKMixName;        // DCOP name for KMix
		QString             mKMixError;       // error message starting KMix
		QTimer*             mPlayTimer;       // timer for repeating the sound file
		QTimer*             mFadeTimer;       // timer for fading the sound volume
		float               mOldVolume;       // volume before volume was set for sound file
		QString             mLocalAudioFile;  // local copy of audio file
		QTime               mAudioFileStart;  // time when audio file loading first started, or when play first started
		int                 mAudioFileLoadSecs;  // how many seconds it took to load audio file
		bool                mPlayedOnce;      // the sound file has started playing at least once
		bool                mPlayed;          // the PlayObject->play() has been called
		// Miscellaneous
		KAEvent             mEvent;           // the whole event, for updating the calendar file
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
		bool                mUsingKMix;       // master volume is being set using kmix
		bool                mDisableDeferral; // true if past deferral limit, so don't enable Defer button
};

#endif // MESSAGEWIN_H
