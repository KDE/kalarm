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

#include <msgevent.h>
using namespace KCal;


/**
 * MessageWin: A window to display an alarm message
 */
class MessageWin : public KMainWindow
{
		Q_OBJECT
	public:
		MessageWin();     // for session management restoration only
		explicit MessageWin(const MessageEvent&, bool delete_event = true);
		~MessageWin();
		virtual void showEvent(QShowEvent*);

	protected:
		virtual void saveProperties(KConfig*);
		virtual void readProperties(KConfig*);

	private:
		QSize             initView();

		QString           message;
		QFont             font;
		QColor            colour;
		QDateTime         dateTime;
		QString           eventID;
		QString           audioFile;
		bool              beep;
		QPushButton*      buttonOK;
		bool              deleteEvent;  // true to delete event after message has been displayed
		bool              shown;        // true once the window has been displayed
};

#endif // MESSAGEWIN_H
