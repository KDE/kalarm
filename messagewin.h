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
 */

#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

#include "mainwindowbase.h"
#include "alarmevent.h"

class QPushButton;
class AlarmTimeWidget;

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
		bool                hasDefer() const       { return !!deferButton; }
		bool                errorMessage() const   { return mErrorMsgs.count(); }
		static int          instanceCount()        { return windowList.count(); }
		static MessageWin*  findEvent(const QString& eventID);

	protected:
		virtual void        showEvent(QShowEvent*);
		virtual void        resizeEvent(QResizeEvent*);
		virtual void        closeEvent(QCloseEvent*);
		virtual void        saveProperties(KConfig*);
		virtual void        readProperties(KConfig*);

	protected slots:
		void                slotDefer();
		void                displayMainWindow();

	private:
		QSize               initView();
		void                playAudio();

		static QPtrList<MessageWin> windowList;  // list of existing message windows
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
		// Miscellaneous
		QPushButton*        deferButton;
		int                 restoreHeight;
		bool                rescheduleEvent;  // true to delete event after message has been displayed
		bool                shown;            // true once the window has been displayed
		bool                deferClosing;     // the Defer button is closing the dialog
		bool                mDeferDlgShowing; // the defer dialog is being displayed
};

#endif // MESSAGEWIN_H
