/*
 *  messagewin.h  -  displays an alarm message
 *  Program:  kalarm
 *  (C) 2001, 2002 by David Jarvie  software@astrojar.org.uk
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
 */

#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

#include "mainwindowbase.h"

#include "msgevent.h"
using namespace KCal;

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
		MessageWin(const QString& errmsg, const KAlarmEvent&, const KAlarmAlarm&, bool reschedule_event = true);
		~MessageWin();
		void               repeat();
		bool               errorMessage() const   { return !errorMsg.isNull(); }
		static int         instanceCount()        { return windowList.count(); }
		static MessageWin* findEvent(const QString& eventID);

	protected:
		virtual void       showEvent(QShowEvent*);
		virtual void       resizeEvent(QResizeEvent*);
		virtual void       saveProperties(KConfig*);
		virtual void       readProperties(KConfig*);

	protected slots:
		void               slotClose();
		void               slotShowDefer();
		void               slotDefer();

	private:
		QSize              initView();

		static QPtrList<MessageWin> windowList;  // list of existing message windows
		// KAlarmEvent properties
		KAlarmEvent        event;            // the whole event, for updating the calendar file
		QString            message;
		QFont              font;
		QColor             colour;
		QDateTime          dateTime;
		QString            eventID;
		QString            audioFile;
		int                alarmID;
		int                flags;
		bool               beep;
		bool               confirmAck;
		bool               dateOnly;         // ignore event's time
		KAlarmAlarm::Type  type;
		QString            errorMsg;
		bool               noDefer;          // don't display a Defer option
		// Miscellaneous
		QPushButton*       deferButton;
		AlarmTimeWidget*   deferTime;
		int                deferHeight;      // height of defer dialog
		int                restoreHeight;
		bool               rescheduleEvent;  // true to delete event after message has been displayed
		bool               shown;            // true once the window has been displayed
		bool               deferDlgShown;    // true if defer dialog is visible
};

#endif // MESSAGEWIN_H
