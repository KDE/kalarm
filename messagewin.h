/*
 *  messagewin.h  -  displays an alarm message
 *  Program:  kalarm
 *  (C) 2001 - 2003 by David Jarvie  software@astrojar.org.uk
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  As a special exception, permission is given to link this program
 *  with any edition of Qt, and distribute the resulting executable,
 *  without including the source code for Qt in the source distribution.
 */

#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

#include <qtextedit.h>

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
		MessageWin(const KAlarmEvent&, const KAlarmAlarm&, bool reschedule_event = true, bool allowDefer = true);
		MessageWin(const KAlarmEvent&, const KAlarmAlarm&, const QString& errmsg,
		           const QString& errmsg2 = QString::null, bool reschedule_event = true);
		~MessageWin();
		void                repeat(const KAlarmAlarm&);
		const DateTime&     dateTime()             { return mDateTime; }
		KAlarmAlarm::Type   alarmType() const      { return mAlarmType; }
		bool                hasDefer() const       { return !!deferButton; }
		bool                errorMessage() const   { return !errorMsg.isNull(); }
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
		// KAlarmEvent properties
		KAlarmEvent         mEvent;           // the whole event, for updating the calendar file
		QString             message;
		QFont               font;
		QColor              colour;
		DateTime            mDateTime;        // date/time displayed in the message window
		QString             eventID;
		QString             audioFile;
		QString             emailAddresses;
		QString             emailSubject;
		KAlarmAlarm::Type   mAlarmType;
		int                 flags;
		bool                beep;
		bool                confirmAck;
		bool                dateOnly;         // ignore event's time
		KAlarmAlarm::Action action;
		QString             errorMsg, errorMsg2;
		bool                noDefer;          // don't display a Defer option
		// Miscellaneous
		QPushButton*        deferButton;
		int                 restoreHeight;
		bool                rescheduleEvent;  // true to delete event after message has been displayed
		bool                shown;            // true once the window has been displayed
		bool                deferClosing;     // the Defer button is closing the dialog
};


class MessageText : public QTextEdit
{
		Q_OBJECT
	public:
		MessageText(const QString& text, const QString& context = QString::null, QWidget* parent = 0, const char* name = 0);
		virtual QSize sizeHint() const;
};

#endif // MESSAGEWIN_H
