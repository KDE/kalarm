/*
 *  messagewin.h  -  displays an alarm message
 *  Program:  kalarm
 *  (C) 2001 - 2004 by David Jarvie <software@astrojar.org.uk>
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
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  In addition, as a special exception, the copyright holders give permission
 *  to link the code of this program with any edition of the Qt library by
 *  Trolltech AS, Norway (or with modified versions of Qt that use the same
 *  license as Qt), and distribute linked combinations including the two.
 *  You must obey the GNU General Public License in all respects for all of
 *  the code used other than Qt.  If you modify this file, you may extend
 *  this exception to your version of the file, but you are not obligated to
 *  do so. If you do not wish to do so, delete this exception statement from
 *  your version.
 */

#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

#include "mainwindowbase.h"
#include "alarmevent.h"

class QPushButton;
class QLabel;
class QTimer;
class AlarmTimeWidget;
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
		MessageWin(const KAEvent&, const KAAlarm&, const QStringList& errmsgs, bool reschedule_event = true);
		~MessageWin();
		void                repeat(const KAAlarm&);
		const DateTime&     dateTime()             { return mDateTime; }
		KAAlarm::Type       alarmType() const      { return mAlarmType; }
		bool                hasDefer() const       { return !!mDeferButton; }
		bool                errorMessage() const   { return mErrorMsgs.count(); }
		static int          instanceCount()        { return mWindowList.count(); }
		static MessageWin*  findEvent(const QString& eventID);

	protected:
		virtual void        showEvent(QShowEvent*);
		virtual void        resizeEvent(QResizeEvent*);
		virtual void        closeEvent(QCloseEvent*);
		virtual void        saveProperties(KConfig*);
		virtual void        readProperties(KConfig*);

	private slots:
		void                slotDefer();
		void                displayMainWindow();
		void                slotPlayAudio();
		void                checkAudioPlay();
		void                stopPlay();
		void                setRemainingTextDay();
		void                setRemainingTextMinute();

	private:
		QSize               initView();
#ifndef WITHOUT_ARTS
		void                initAudio();
#endif
		void                playAudio();

		static QPtrList<MessageWin> mWindowList;  // list of existing message windows
		// KAEvent properties
		KAEvent             mEvent;           // the whole event, for updating the calendar file
		QString             message;
		QFont               font;
		QColor              mBgColour, mFgColour;
		DateTime            mDateTime;        // date/time displayed in the message window
		QString             eventID;
		KAAlarm::Type       mAlarmType;
		int                 flags;
		bool                beep;
		bool                confirmAck;
		bool                dateOnly;         // ignore event's time
		KAAlarm::Action     action;
		QStringList         mErrorMsgs;
		bool                noDefer;          // don't display a Defer option
		// Sound file playing
		KArtsDispatcher*    mArtsDispatcher;
		KDE::PlayObject*    mPlayObject;
		QTimer*             mPlayTimer;       // timer for repeating the sound file
		bool                mPlayedOnce;      // the sound file has been played at least once
		bool                mPlayed;          // the PlayObject->play() has been called
		// Miscellaneous
		QLabel*             mRemainingText;   // the remaining time (for a reminder window)
		QPushButton*        mDeferButton;
		QPushButton*        mSilenceButton;
		QString             mLocalAudioFile;  // local copy of audio file
		QTime               mAudioFileLoadStart; // time when audio file loading started
		int                 mAudioFileLoadSecs;  // how many seconds it took to load audio file
		int                 mRestoreHeight;
		bool                mRescheduleEvent; // true to delete event after message has been displayed
		bool                mShown;           // true once the window has been displayed
		bool                mDeferClosing;    // the Defer button is closing the dialog
		bool                mDeferDlgShowing; // the defer dialog is being displayed
};

#endif // MESSAGEWIN_H
