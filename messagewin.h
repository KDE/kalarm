/*
 *  messagewin.h  -  displays an alarm message
 *  Program:  kalarm
 *
 *  (C) 2001 by David Jarvie  software@astrojar.org.uk
 *
 *  This program is free software; you can redistribute it and/or modify  *
 *  it under the terms of the GNU General Public License as published by  *
 *  the Free Software Foundation; either version 2 of the License, or     *
 *  (at your option) any later version.                                   *
 */

#ifndef MESSAGEWIN_H
#define MESSAGEWIN_H

#include <kmainwindow.h>

#include "msgevent.h"
using namespace KCal;

class QPushButton;
class AlarmTimeWidget;

/**
 * MessageWin: A window to display an alarm message
 */
class MessageWin : public KMainWindow
{
		Q_OBJECT
	public:
		MessageWin();     // for session management restoration only
		explicit MessageWin(const MessageEvent&, bool reschedule_event = true);
		~MessageWin();

	protected:
		virtual void showEvent(QShowEvent*);
		virtual void resizeEvent(QResizeEvent*);
		virtual void saveProperties(KConfig*);
		virtual void readProperties(KConfig*);

	protected slots:
		void              slotShowDefer();
		void              slotDefer();
		void              slotKAlarm();

	private:
		QSize             initView();
		// MessageEvent properties
		QString           message;
		QFont             font;
		QColor            colour;
		QDateTime         dateTime;
		QString           eventID;
		int               repeats;
		int               flags;
		QString           audioFile;
		bool              beep;
		bool              file;
		// Miscellaneous
		QPushButton*      deferButton;
		AlarmTimeWidget*  deferTime;
		bool              rescheduleEvent;  // true to delete event after message has been displayed
		bool              shown;            // true once the window has been displayed
		bool              deferDlgShown;    // true if defer dialog is visible
		bool              fileError;        // true if initView() couldn't open the file to display
};

#endif // MESSAGEWIN_H
